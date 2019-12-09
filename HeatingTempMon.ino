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

// Setup 3 oneWire instances to communicate with temp sensors
OneWire oneWireUpstairs(0);
OneWire oneWireDownstairs(2);
OneWire oneWireBoilerAndValve(4);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensorsUpstairs(&oneWireUpstairs);
DallasTemperature sensorsDownstairs(&oneWireDownstairs);
DallasTemperature sensorsBoilerAndValve(&oneWireBoilerAndValve);

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


void setupDevice(DallasTemperature sensors, DeviceAddress device)
{
#ifdef DEBUG
  Serial.print("Device Address: ");
  printAddress(device);
  Serial.println();
#endif

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(device, 9);

#ifdef DEBUG
  Serial.print("Device Resolution: ");
  Serial.print(sensors.getResolution(device), DEC);
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
  
  sensorsUpstairs.begin();
  sensorsDownstairs.begin();
  sensorsBoilerAndValve.begin();

  // setup devices
  for (int x = 0; x < 6; ++x)
  {
    setupDevice(sensorsUpstairs, devicesUpstairs[x].devaddr);
  }
  for (int x = 0; x < 8; ++x)
  {
    setupDevice(sensorsDownstairs, devicesDownstairs[x].devaddr);
  }
  for (int x = 0; x < 4; ++x)
  {
    setupDevice(sensorsBoilerAndValve, devicesBoilerAndValve[x].devaddr);
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


void processTemp(String tstr, DallasTemperature sensors, devinfo &d)
{
  WiFiClient client;
  HTTPClient http;

  float tempC = sensors.getTempC(d.devaddr);
  float tempF = sensors.toFahrenheit(tempC);
#ifdef DEBUG
  printTemperature(d.devaddr, tempC, tempF); // Use a simple function to print out the data
#endif
  DEBUG_PRINTLN("[HTTP] begin...");
  String url = String("http://") + HOST + ":" + PORT + "/sensors/" + d.devname + "/data";
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
    DEBUG_PRINTF("\n\ntimestamp = %lu\n\n", etime);
    sensorsUpstairs.requestTemperatures(); // Send the command to get temperatures
    sensorsDownstairs.requestTemperatures(); // Send the command to get temperatures
    sensorsBoilerAndValve.requestTemperatures(); // Send the command to get temperatures

    DateTime ldtm(etime);
    char buffer[] = "YYYY-MM-DD-hh-mm-ss";
    ldtm.toString(buffer);
    DEBUG_PRINTF("\n\ntimestr = %s\n\n", buffer);

    for (int x = 0; x < 6; ++x)
    {
      processTemp(buffer, sensorsUpstairs, devicesUpstairs[x]);
    }
    for (int x = 0; x < 8; ++x)
    {
      processTemp(buffer, sensorsDownstairs, devicesDownstairs[x]);
    }
    for (int x = 0; x < 4; ++x)
    {
      processTemp(buffer, sensorsBoilerAndValve, devicesBoilerAndValve[x]);
    }
  }
}
