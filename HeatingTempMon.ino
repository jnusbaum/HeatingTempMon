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

#include "DeviceAddresses.h"

#define HOST "192.168.0.134"
#define ROOT "/dataserver"

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
  String devname;
  DeviceAddress *devaddr;
}
devinfo;

// temp sensor buss
typedef struct {
  int pin;
  DallasTemperature *bus;
  int numsensors;
  devinfo **sensors;
}
sensorbus;


WiFiClient net;
MQTTClient client;


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "us.pool.ntp.org", -25200, 5000000);

String baseTopic = "sorrelhills/temperature/";

// set room config
#define MECHROOM

// Set up onewire and sensors
#ifdef MECHROOM

const char mqtt_client_id[] = "MECHROOM";

devinfo laundry_in = { "LAUNDRY-IN", &dev_one }; // 1
devinfo kitchen_in = { "KITCHEN-IN", &dev_two }; // 2
devinfo garage_in = { "GARAGE-IN", &dev_three }; // 3
devinfo kitchen_out = { "KITCHEN-OUT", &dev_four }; // 4
devinfo garage_out = { "GARAGE-OUT", &dev_five }; // 5
devinfo laundry_out = { "LAUNDRY-OUT", &dev_six }; // 6
devinfo valve_out = { "VALVE-OUT", &dev_seven }; // 7
devinfo valve_insys = { "VALVE-INSYS", &dev_eight }; // 8
devinfo boiler_out = { "BOILER-OUT", &dev_nine }; // 9
devinfo boiler_in = { "BOILER-IN", &dev_ten }; // 10
devinfo family_out = { "FAMILY-OUT", &dev_eleven }; // 11
devinfo office_out = { "OFFICE-OUT", &dev_twelve }; // 12
devinfo exercise_out = { "EXERCISE-OUT", &dev_thirteen }; // 13
devinfo guest_out = { "GUEST-OUT", &dev_fourteen }; // 14
devinfo family_in = { "FAMILY-IN", &dev_fifteen }; // 15
devinfo office_in = { "OFFICE-IN", &dev_sixteen }; // 16
devinfo exercise_in = { "EXERCISE-IN", &dev_seventeen }; // 17
devinfo guest_in = { "GUEST-IN", &dev_eighteen }; // 18

devinfo *devicesUpstairs[] = { &kitchen_out, &laundry_out, &garage_out, &laundry_in, &garage_in, &kitchen_in };
OneWire oneWireUpstairs(0);
DallasTemperature sensorsUpstairs(&oneWireUpstairs);
sensorbus busUpstairs = { 0, &sensorsUpstairs, 6, devicesUpstairs };

devinfo *devicesDownstairs[] = { &family_out, &office_out, &exercise_out, &guest_out, &family_in, &office_in, &exercise_in, &guest_in };
OneWire oneWireDownstairs(2);
DallasTemperature sensorsDownstairs(&oneWireDownstairs);
sensorbus busDownstairs = { 2, &sensorsDownstairs, 8, devicesDownstairs };

devinfo *devicesBoilerAndValve[] = { &boiler_in, &boiler_out, &valve_insys, &valve_out };
OneWire oneWireBoilerAndValve(4);
DallasTemperature sensorsBoilerAndValve(&oneWireBoilerAndValve);
sensorbus busBoilerAndValve = { 4, &sensorsBoilerAndValve, 4, devicesBoilerAndValve };

int numbusses = 3;
sensorbus *busses[] = { &busUpstairs, &busDownstairs, &busBoilerAndValve };

#endif



#ifdef CLOSET

const char mqtt_client_id[] = "CLOSET";

devinfo office_in = { "LIBRARY-IN", &dev_twentyfour }; // 24
devinfo mbr_in = { "MBR-IN", &dev_twentytwo }; // 22
devinfo mbath_in = { "MBATH-IN", &dev_twentythree }; // 23
devinfo office_out = { "LIBRARY-OUT", &dev_twentyone }; // 21
devinfo mbr_out = { "MBR-OUT", &dev_twenty }; // 20
devinfo mbath_out = { "MBATH-OUT", &dev_nineteen }; // 19

devinfo *devicesCloset[] = { &office_in, &mbr_in, &mbath_in, &office_out, &mbr_out, &mbath_out };
OneWire oneWireCloset(2);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensorsCloset(&oneWireCloset);
sensorbus busCloset = { 2, &sensorsCloset, 6, devicesCloset };

int numbusses = 1;
sensorbus *busses[] = { &busCloset };

#endif


#ifdef DEBUG

// function to print a device address
// only called when DEBUG
void printAddress(DeviceAddress *deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if ((*deviceAddress)[i] < 16) Serial.print("0");
    Serial.print((*deviceAddress)[i], HEX);
  }
}


// function to print the temperature for a device
// only called when DEBUG
void printTemperature(DeviceAddress *d, float tempC, float tempF)
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


void setupDevices(sensorbus *bus)
{
  for (int y = 0; y < bus->numsensors; ++y)
  {
  #ifdef DEBUG
    Serial.print("Device Address: ");
    printAddress(bus->sensors[y]->devaddr);
    Serial.println();
  #endif

    // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
    bus->bus->setResolution(*(bus->sensors[y]->devaddr), 9);

  #ifdef DEBUG
    Serial.print("Device Resolution: ");
    Serial.print(bus->bus->getResolution(*(bus->sensors[y]->devaddr)), DEC);
    Serial.println();
  #endif
  }
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
  while (!client.connect(mqtt_client_id)) {
    DEBUG_PRINT(".");
    delay(500);
  }
  DEBUG_PRINTLN("\nconnected!");
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
  client.begin(HOST, net);

  for (int x = 0; x < numbusses; ++x)
  {
    busses[x]->bus->begin();
    setupDevices(busses[x]);
  }

  connect();

  timeClient.begin();
  timeClient.forceUpdate();
}


void processTemps(sensorbus *bus, char *tstr)
{
  for (int y = 0; y < bus->numsensors; ++y)
  {
    float tempC = bus->bus->getTempC(*(bus->sensors[y]->devaddr));
    float tempF = bus->bus->toFahrenheit(tempC);
  #ifdef DEBUG
    printTemperature(bus->sensors[y]->devaddr, tempC, tempF); // Use a simple function to print out the data
  #endif

    StaticJsonDocument<128> doc;
    doc["sensor"] = bus->sensors[y]->devname;
    doc["timestamp"] = tstr;
    doc["value"] = tempF;
    // Generate the minified JSON and put it in buffer.
    String topic = baseTopic + bus->sensors[y]->devname;
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

  if (!client.connected()) 
  {
    connect();
  }
  
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > period)
  {
    previousMillis = currentMillis;

    // call sensors.requestTemperatures() to issue a global temperature
    unsigned long etime = timeClient.getEpochTime();
    for (int x = 0; x < numbusses; ++x)
    {
      busses[x]->bus->requestTemperatures();
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
