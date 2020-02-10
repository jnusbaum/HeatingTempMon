#/*
  Name:		HeatingTempMon.ino
  Created:	10/28/2019 2:46:05 PM
  Author:	nusbaum
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <MQTT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <OneWire.h>
#include <ArduinoJson.h>
#include <DallasTemperature.h>

#define DEVICENAME "TEST";
#define MQTTHOST "192.168.0.134"

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x)     Serial.print (x)
#define DEBUG_PRINTF(x, y)     Serial.printf (x, y)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTF(x, y)
#define DEBUG_PRINTLN(x)
#endif

// device addresses
typedef struct {
  char devname[64];
  DeviceAddress devaddr;
}
devinfo;

// temp sensor buss
typedef struct {
  int pin;
  OneWire *wire;
  DallasTemperature *bus;
  int numsensors;
  devinfo *sensors;
}
sensorbus;


WiFiClient net;
MQTTClient client(4096);

String mqtt_client_id;
String baseTopic = "sorrelhills/";
String tempTopic = baseTopic + "temperature/";
String configRequestTopic = baseTopic + "device/config-request/" + DEVICENAME;
String configReceiveTopic = baseTopic + "device/config/" + DEVICENAME;
int numbusses = 0;
sensorbus *busses = nullptr;
bool configured = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "us.pool.ntp.org", -25200, 5000000);


void parseAddress(DeviceAddress &daddress, const char *caddress) {
  daddress[0] = (uint8_t)strtoul(caddress, nullptr, 16);
  daddress[1] = (uint8_t)strtoul(caddress+6, nullptr, 16);
  daddress[2] = (uint8_t)strtoul(caddress+12, nullptr, 16);
  daddress[3] = (uint8_t)strtoul(caddress+18, nullptr, 16);
  daddress[4] = (uint8_t)strtoul(caddress+24, nullptr, 16);
  daddress[5] = (uint8_t)strtoul(caddress+30, nullptr, 16);
  daddress[6] = (uint8_t)strtoul(caddress+36, nullptr, 16);
  daddress[7] = (uint8_t)strtoul(caddress+42, nullptr, 16);
}

#ifdef DEBUG

// function to print a device address
// only called when DEBUG
void printAddress(DeviceAddress &deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}


// function to print the temperature for a device
// only called when DEBUG
void printTemperature(DeviceAddress &d, float tempC, float tempF)
{
  Serial.print("Temp for Address: ");
  printAddress(d);
  Serial.println();

  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.println(tempF);
}

#endif


void setupDevices(sensorbus &bus)
{
  bus.bus->begin();
  for (int y = 0; y < bus.numsensors; ++y)
  {
  #ifdef DEBUG
    Serial.print("Device Address: ");
    printAddress(bus.sensors[y].devaddr);
    Serial.println();
  #endif

    // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
    bus.bus->setResolution(bus.sensors[y].devaddr, 9);

  #ifdef DEBUG
    Serial.print("Device Resolution: ");
    Serial.print(bus.bus->getResolution(bus.sensors[y].devaddr), DEC);
    Serial.println();
  #endif
  }
}


void configReceived(String &topic, String &payload) {
  DEBUG_PRINTLN("incoming config: " + topic + " - " + payload);
  DynamicJsonDocument doc(4096);

  // shutdown and free existing config here
  for (int x = 0; x < numbusses; ++x) {
    free(busses[x].sensors);
    delete busses[x].bus;
    delete busses[x].wire; 
  }
  if (busses) free(busses);
    
  // create new config and setup
  DeserializationError error = deserializeJson(doc, payload.c_str());
  DEBUG_PRINTLN(String("deserialization result: ") + error.c_str());
  mqtt_client_id = doc["client_id"].as<const char*>();
  DEBUG_PRINTLN(String("client id: ") + mqtt_client_id);
  numbusses = doc["num_interfaces"];
  DEBUG_PRINTLN(String("number of interfaces: ") + numbusses);
  busses = (sensorbus *)malloc(sizeof(sensorbus) * numbusses);
  JsonArray interfaces = doc["interfaces"].as<JsonArray>();
  for (int x = 0; x < numbusses; ++x) {
    JsonObject jbus = interfaces[x];
    const int pin_number = jbus["pin_number"];
    DEBUG_PRINTLN(String("pin number: ") + pin_number);
    const int num_sensors = jbus["num_sensors"];
    DEBUG_PRINTLN(String("number of sensors: ") + num_sensors);
    busses[x].pin = pin_number;
    busses[x].wire = new OneWire(pin_number);
    busses[x].bus = new DallasTemperature(busses[x].wire);
    busses[x].numsensors = num_sensors;
    busses[x].sensors = (devinfo *)malloc(sizeof(devinfo) * num_sensors);
    JsonArray sensors = jbus["sensors"].as<JsonArray>();
    for (int y = 0; y < num_sensors; ++y) {
      JsonObject jsensor = sensors[y];
      strcpy(busses[x].sensors[y].devname, jsensor["name"].as<const char*>());
      DEBUG_PRINTLN(String("sensor name: ") + busses[x].sensors[y].devname);
      // parse string into device address
      const char *daddress = jsensor["address"].as<const char*>();
      DEBUG_PRINTLN(String("sensor address: ") + daddress);
      parseAddress(busses[x].sensors[y].devaddr, daddress);
    }
    setupDevices(busses[x]);
  }

  configured = true;
}


void req_configure() {
  DEBUG_PRINTLN("configuring...");
  configured = false;
  client.publish(configRequestTopic.c_str());
  DEBUG_PRINTLN("config requested");
}


void connect() {
  DEBUG_PRINT("Wait for WiFi... ");
  while (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINT(".");
    delay(500);
  }
  
  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
  DEBUG_PRINTLN("MAC address: ");
  DEBUG_PRINTLN(WiFi.macAddress());

  DEBUG_PRINT("\nconnecting to MQTT...");
  while (!client.connect(mqtt_client_id.c_str())) {
    DEBUG_PRINT(".");
    delay(500);
  }
  DEBUG_PRINTLN("\nconnected!");
  
  client.subscribe(configReceiveTopic.c_str());
  DEBUG_PRINTLN(String("subscribed to ") + configReceiveTopic);
}


void setup() {
#ifdef DEBUG
  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    DEBUG_PRINTF("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin("nusbaum-24g", "we live in Park City now");
  client.begin(MQTTHOST, net);
  client.onMessage(configReceived);



  connect();

  timeClient.begin();
  timeClient.forceUpdate();

  req_configure();
}


void processTemps(sensorbus &bus, char *tstr)
{
  for (int y = 0; y < bus.numsensors; ++y)
  {
    float tempC = bus.bus->getTempC(bus.sensors[y].devaddr);
    float tempF = bus.bus->toFahrenheit(tempC);
  #ifdef DEBUG
    printTemperature(bus.sensors[y].devaddr, tempC, tempF); // Use a simple function to print out the data
  #endif

    StaticJsonDocument<128> doc;
    doc["sensor"] = bus.sensors[y].devname;
    doc["timestamp"] = tstr;
    doc["value"] = tempF;
    // Generate the minified JSON and put it in buffer.
    String topic = tempTopic + bus.sensors[y].devname;
    char buffer[128];
    int n = serializeJson(doc, buffer);
    DEBUG_PRINTF("[MQTT] PUBLISHing to %s\n", topic.c_str());
    client.publish(topic.c_str(), buffer, n);
  }
}


int period = 10000;
unsigned long previousMillis = 0;

void loop() 
{
  client.loop();
  delay(10);
  
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > period)
  {
    if (!client.connected()) connect();

    previousMillis = currentMillis;

    // call sensors.requestTemperatures() to issue a global temperature
    unsigned long etime = timeClient.getEpochTime();
    for (int x = 0; x < numbusses; ++x)
    {
      busses[x].bus->requestTemperatures();
    }
    DEBUG_PRINTF("\n\ntimestamp = %lu\n\n", etime);
    
    DateTime ldtm(etime);
    char tstr[] = "YYYY-MM-DDThh:mm:ss";
    ldtm.toString(tstr);
    DEBUG_PRINTF("\n\ntimestr = %s\n\n", tstr);

    for (int x = 0; x < numbusses; ++x)
    {
      processTemps(busses[x], tstr);
    }
  }
}
