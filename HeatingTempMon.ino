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



WiFiClient net;
MQTTClient client(4096);



String mqtt_client_id;
String baseTopic = "sorrelhills/";
String tempTopic = baseTopic + "temperature/";
String statusTopic = baseTopic + "device/status/" + DEVICENAME;
String configRequestTopic = baseTopic + "device/config-request/" + DEVICENAME;
String configReceiveTopic = baseTopic + "device/config/" + DEVICENAME;
bool configured = false;
String isofmtstr = "YYYY-MM-DDThh:mm:ss";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "us.pool.ntp.org", -25200, 5000000);

// get current time as iso format string
void getCurrentTime(char *tstr) 
{
  unsigned long etime = timeClient.getEpochTime();
  strcpy(tstr, isofmtstr.c_str());
  DateTime ldtm(etime);
  ldtm.toString(tstr);
}


void publishStatus(const char *statusstr)
{
  StaticJsonDocument<128> doc;
  char tstr[isofmtstr.length()];
  getCurrentTime(tstr);
  doc["status"] = statusstr;
  doc["timestamp"] = tstr;
  char buffer[128];
  int n = serializeJson(doc, buffer);
  DEBUG_PRINTF("[MQTT] PUBLISHing to %s\n", statusTopic.c_str());
  client.publish(statusTopic.c_str(), buffer, n);
  DEBUG_PRINTLN("Published.");
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



class TempSensor {
    char devname[64];
    DeviceAddress devaddr;

  public:
    void initialize(const char *i_devname, const char *i_daddress) {
      strcpy(devname, i_devname);
      devaddr[0] = (uint8_t)strtoul(i_daddress, nullptr, 16);
      devaddr[1] = (uint8_t)strtoul(i_daddress+6, nullptr, 16);
      devaddr[2] = (uint8_t)strtoul(i_daddress+12, nullptr, 16);
      devaddr[3] = (uint8_t)strtoul(i_daddress+18, nullptr, 16);
      devaddr[4] = (uint8_t)strtoul(i_daddress+24, nullptr, 16);
      devaddr[5] = (uint8_t)strtoul(i_daddress+30, nullptr, 16);
      devaddr[6] = (uint8_t)strtoul(i_daddress+36, nullptr, 16);
      devaddr[7] = (uint8_t)strtoul(i_daddress+42, nullptr, 16);
    }

    const char *device_name() {
      return devname;
    }

    DeviceAddress &device_address() {
      return devaddr;
    }
};


class SensorBus {
    int pin;
    OneWire *wire;
    DallasTemperature *bus;
    int numsensors;
    TempSensor *sensors;

  public:
    SensorBus() {
      pin = 0;
      wire = nullptr;
      bus = nullptr;
      numsensors = 0;
      sensors = nullptr;
    }

    void initialize(const int i_pin, const int i_numsensors) {
      pin = i_pin;
      wire = new OneWire(pin);
      bus = new DallasTemperature(wire);
      numsensors = i_numsensors;
      sensors = new TempSensor[numsensors];
    }

    void initsensor(const int i_sensoridx, const char *i_devname, const char *i_daddress) {
      sensors[i_sensoridx].initialize(i_devname, i_daddress);
    }

    void begin() {
      bus->begin();
      for (int y = 0; y < numsensors; ++y)
      {
        #ifdef DEBUG
        Serial.print("Device Address: ");
        printAddress(sensors[y].device_address());
        Serial.println();
        #endif

        // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
        bus->setResolution(sensors[y].device_address(), 9);

        #ifdef DEBUG
        Serial.print("Device Resolution: ");
        Serial.print(bus->getResolution(sensors[y].device_address()), DEC);
        Serial.println();
        #endif
      }
    }

    void requestTemps() {
      bus->requestTemperatures();
    }

    void processTemps(char *tstr) {
      for (int y = 0; y < numsensors; ++y)
      {
        float tempC = bus->getTempC(sensors[y].device_address());
        float tempF = bus->toFahrenheit(tempC);
      #ifdef DEBUG
        printTemperature(sensors[y].device_address(), tempC, tempF); // Use a simple function to print out the data
      #endif

        StaticJsonDocument<128> doc;
        doc["sensor"] = sensors[y].device_name();
        doc["timestamp"] = tstr;
        doc["value"] = tempF;
        // Generate the minified JSON and put it in buffer.
        String topic = tempTopic + sensors[y].device_name();
        char buffer[128];
        int n = serializeJson(doc, buffer);
        DEBUG_PRINTF("[MQTT] PUBLISHing to %s\n", topic.c_str());
        client.publish(topic.c_str(), buffer, n);
      }
    }

    ~SensorBus() {
      delete [] sensors;
      delete bus;
      delete wire;
    }

};


int numbusses = 0;
SensorBus *busses = nullptr;


void configReceived(String &topic, String &payload) {
  DEBUG_PRINTLN("incoming config: " + topic + " - " + payload);
  publishStatus("CONFIG RECEIVED");
  DynamicJsonDocument doc(4096);

  // shutdown and free existing config here
  if (busses) delete [] busses;
    
  // create new config and setup
  DeserializationError error = deserializeJson(doc, payload.c_str());
  DEBUG_PRINTLN(String("deserialization result: ") + error.c_str());
  mqtt_client_id = doc["client_id"].as<const char*>();
  DEBUG_PRINTLN(String("client id: ") + mqtt_client_id);
  numbusses = doc["num_interfaces"];
  DEBUG_PRINTLN(String("number of interfaces: ") + numbusses);
  busses = new SensorBus[numbusses];
  JsonArray interfaces = doc["interfaces"].as<JsonArray>();
  for (int x = 0; x < numbusses; ++x) {
    JsonObject jbus = interfaces[x];
    const int pin_number = jbus["pin_number"];
    DEBUG_PRINTLN(String("pin number: ") + pin_number);
    const int num_sensors = jbus["num_sensors"];
    DEBUG_PRINTLN(String("number of sensors: ") + num_sensors);
    busses[x].initialize(pin_number, num_sensors);
    JsonArray sensors = jbus["sensors"].as<JsonArray>();
    for (int y = 0; y < num_sensors; ++y) {
      JsonObject jsensor = sensors[y];
      const char *devname = jsensor["name"].as<const char*>();
      DEBUG_PRINTLN(String("sensor name: ") + devname);
      const char *daddress = jsensor["address"].as<const char*>();
      DEBUG_PRINTLN(String("sensor address: ") + daddress);
      busses[x].initsensor(y, devname, daddress);
    }
    busses[x].begin();
  }

  configured = true;
  publishStatus("CONFIGURED");
}


void req_configure() {
  DEBUG_PRINTLN("configuring...");
  configured = false;
  client.publish(configRequestTopic.c_str());
  DEBUG_PRINTLN("config requested");
  publishStatus("CONFIG REQUESTED");
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

  publishStatus("STARTING");

  req_configure();
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
    if (!client.connected()) 
    {
      connect();
      publishStatus("RECONNECTED");
    }

    previousMillis = currentMillis;

    char tstr[isofmtstr.length()];
    getCurrentTime(tstr);
    for (int x = 0; x < numbusses; ++x)
    {
      busses[x].requestTemps();
    }
    DEBUG_PRINTF("timestr = %s\n", tstr);
    delay(500);
    
    for (int x = 0; x < numbusses; ++x)
    {
      busses[x].processTemps(tstr);
    }
    publishStatus("RUNNING");
  }
}
