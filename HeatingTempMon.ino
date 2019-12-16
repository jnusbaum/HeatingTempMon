#/*
  Name:		HeatingTempMon.ino
  Created:	10/28/2019 2:46:05 PM
  Author:	nusbaum
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <OneWire.h>
#include "DeviceAddresses.h"

#define HOST "192.168.0.134"
#define PORT "80"

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
  devinfo *sensors;
}
sensorbus;


#define MECHROOM

// Set up onewire and sensors
#ifdef MECHROOM

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

devinfo devicesUpstairs[] = { kitchen_out, laundry_out, garage_out, laundry_in, garage_in, kitchen_in };
OneWire oneWireUpstairs(0);
DallasTemperature sensorsUpstairs(&oneWireUpstairs);
sensorbus busUpstairs = { 0, &sensorsUpstairs, 6, devicesUpstairs };

devinfo devicesDownstairs[] = { family_out, office_out, exercise_out, guest_out, family_in, office_in, exercise_in, guest_in };
OneWire oneWireDownstairs(2);
DallasTemperature sensorsDownstairs(&oneWireDownstairs);
sensorbus busDownstairs = { 2, &sensorsDownstairs, 8, devicesDownstairs };

devinfo devicesBoilerAndValve[] = { boiler_in, boiler_out, valve_insys, valve_out };
OneWire oneWireBoilerAndValve(4);
DallasTemperature sensorsBoilerAndValve(&oneWireBoilerAndValve);
sensorbus busBoilerAndValve = ( 4, &sensorsBoilerAndValve, 4, devicesBoilerAndValve };

numbusses = 3;
sensorbus busses[] = { busUpstairs, busDownstairs, busBoilerAndValve };

#endif



#ifdef CLOSET

devinfo nineteen = { "NINETEEN", dev_nineteen }; // 19
devinfo twenty = { "TWENTY", dev_twenty }; // 20
devinfo twentyone = { "TWENTYONE", dev_twentyone }; // 21
devinfo twentytwo = { "TWENTYTWO", dev_twentytwo }; // 22
devinfo twentythree = { "TWENTYTHREE", dev_twentythree }; // 23
devinfo twentyfour = { "TWENTYFOUR", dev_twentyfour }; // 24
devinfo twentyfive = { "TWENTYFIVE", dev_twentyfive }; // 25

devinfo devicesUpstairs[] = { kitchen_out, laundry_out, garage_out, laundry_in, garage_in, kitchen_in };
devinfo devicesDownstairs[] = { family_out, office_out, exercise_out, guest_out, family_in, office_in, exercise_in, guest_in };
devinfo devicesBoilerAndValve[] = { boiler_in, boiler_out, valve_insys, valve_out };

numsensors = 3;
DallasTemperature tsensors[3];
// Setup 3 oneWire instances to communicate with temp sensors
OneWire oneWireUpstairs(0);
OneWire oneWireDownstairs(2);
OneWire oneWireBoilerAndValve(4);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensorsUpstairs(&oneWireUpstairs);
DallasTemperature sensorsDownstairs(&oneWireDownstairs);
DallasTemperature sensorsBoilerAndValve(&oneWireBoilerAndValve);

#endif


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "us.pool.ntp.org", -25200, 5000000);

#ifdef DEBUG

// function to print a device address
// only called when DEBUG
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}


// function to print the temperature for a device
// only called when DEBUG
void printTemperature(DeviceAddress d, float tempC, float tempF)
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
  for (int y = 0; y < bus.numsensors; ++y)
  {
#ifdef DEBUG
  Serial.print("Device Address: ");
  printAddress(bus.sensors[y].devaddr);
  Serial.println();
#endif

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors->setResolution(bus.sensors[y].devaddr, 9);

#ifdef DEBUG
  Serial.print("Device Resolution: ");
  Serial.print(sensors->getResolution(bus.sensors[y].devaddr), DEC);
  Serial.println();
#endif
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

  WiFi.begin("nusbaum-24g", "we live in Park City now");

  for (int x = 0; x < numbusses; ++x)
  {
    busses[x].bus->begin();
    setupDevices(busses[x]);
  }
    
  DEBUG_PRINT("Wait for WiFi... ");
  
  while (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINT(".");
    delay(500);
  }
  
  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
  
  timeClient.begin();
  timeClient.forceUpdate();
}


void processTemps(sensorbus &bus)
{
  WiFiClient client;
  HTTPClient http;

  for (y = 0; y < bus.numsensors; ++y)
  {
    float tempC = bus.bus->getTempC(bus.sensors[y].devaddr);
    float tempF = bus.bus->toFahrenheit(tempC);
  #ifdef DEBUG
    printTemperature(bus.sensors[y].devaddr, tempC, tempF); // Use a simple function to print out the data
  #endif
    DEBUG_PRINTLN("[HTTP] begin...");
    String url = String("http://") + HOST + ":" + PORT + "/sensors/" + bus.sensors[y].devname + "/data";
    DEBUG_PRINTF("[HTTP] POSTing to %s\n", url.c_str());
    if (http.begin(client, url))
    {
      // HTTP
      DEBUG_PRINTLN("[HTTP] POST...");
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      String data = String("value-real=") + String(tempF, 2) + String("&") + String("timestamp=") + tstr;
      // start connection and send HTTP header
      int httpCode = http.POST(data);
  
      // httpCode will be negative on error
      if (httpCode > 0)
      {
        // HTTP header has been send and Server response header has been handled
        DEBUG_PRINTF("[HTTP] POST... code: %d\n", httpCode);
  
        // file found at server
        if (httpCode != HTTP_CODE_CREATED)
        {
          DEBUG_PRINTF("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
      }
      else
      {
        DEBUG_PRINTF("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
  
      http.end();
    }
    else
    {
      DEBUG_PRINTLN("[HTTP} Unable to connect");
    }
  }
}


int period = 10000;
unsigned long previousMillis = 0;

void loop() 
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > period)
  {
    previousMillis = currentMillis;

    // call sensors.requestTemperatures() to issue a global temperature
    unsigned long etime = timeClient.getEpochTime();
    for (int x = 0; x < numbusses; ++x)
    {
      busses[x].bus->requestTemperatures();
    }
    DEBUG_PRINTF("\n\ntimestamp = %lu\n\n", etime);
    
    DateTime ldtm(etime);
    char buffer[] = "YYYY-MM-DD-hh-mm-ss";
    ldtm.toString(buffer);
    DEBUG_PRINTF("\n\ntimestr = %s\n\n", buffer);

    for (int x = 0; x < numbusses; ++x)
    {
      processTemps(busses[x]);
    }
  }
}
