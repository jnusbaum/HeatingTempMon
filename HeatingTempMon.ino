#/*
 Name:		HeatingTempMon.ino
 Created:	10/28/2019 2:46:05 PM
 Author:	nusbaum
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include <OneWire.h>
#include "DeviceAddresses.h"

ESP8266WiFiMulti WiFiMulti;

// Setup 3 oneWire instances to communicate with temp sensors
OneWire oneWireUpstairs(0);
OneWire oneWireDownstairs(2);
OneWire oneWireBoilerAndValve(4);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensorsUpstairs(&oneWireUpstairs);
DallasTemperature sensorsDownstairs(&oneWireDownstairs);
DallasTemperature sensorsBoilerAndValve(&oneWireBoilerAndValve);


// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}


// function to print the temperature for a device
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


void setupDevice(DallasTemperature sensors, DeviceAddress device)
{
  	Serial.print("Device Address: ");
  	printAddress(device);
  	Serial.println();
  
  	// set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  	sensors.setResolution(device, 9);
  
  	Serial.print("Device Resolution: ");
  	Serial.print(sensors.getResolution(device), DEC);
  	Serial.println();
}


void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("nusbaum-24g", "we live in Park City now");

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
}


void processTemp(DallasTemperature sensors, daddr &d)
{
    WiFiClient client;
    HTTPClient http;
    
    // It responds almost immediately. Let's print out the data
    float tempC = sensors.getTempC(d.devaddr);
    float tempF = sensors.toFahrenheit(tempC);
    printTemperature(d.devaddr, tempC, tempF); // Use a simple function to print out the data
    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, "http://192.168.0.174:8000/api/heating/sensors/" + d.devname + "/data")) 
    {  
      // HTTP
      Serial.print("[HTTP] POST...\n");
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      String data = String("value-real=") + String(tempF, 2);
      // start connection and send HTTP header
      int httpCode = http.POST(data);

      // httpCode will be negative on error
      if (httpCode > 0)
      {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] POST... code: %d\n", httpCode);

        // file found at server
        if (httpCode != HTTP_CODE_OK) 
        {
          Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
      } 
      else 
      {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    } 
    else 
    {
      Serial.printf("[HTTP} Unable to connect\n");
    }
}

    
void loop() {
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) 
  {
    // call sensors.requestTemperatures() to issue a global temperature 
    // request to all devices on the bus
    Serial.print("Requesting temperatures...");
    sensorsUpstairs.requestTemperatures(); // Send the command to get temperatures
    sensorsDownstairs.requestTemperatures(); // Send the command to get temperatures
    sensorsBoilerAndValve.requestTemperatures(); // Send the command to get temperatures
    Serial.println("Done");

    for (int x = 0; x < 6; ++x)
    {
      processTemp(sensorsUpstairs, devicesUpstairs[x]);
    }
    for (int x = 0; x < 8; ++x)
    {
      processTemp(sensorsDownstairs, devicesDownstairs[x]);
    }
    for (int x = 0; x < 4; ++x)
    {
      processTemp(sensorsBoilerAndValve, devicesBoilerAndValve[x]);
    }
  }

  delay(10000);
}
