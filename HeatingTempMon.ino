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
#include <DallasTemperature.h>

#include "DeviceAddresses.h"

ESP8266WiFiMulti WiFiMulti;

#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress one, two, three, four, five;

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


void setupDevice(DeviceAddress d)
{
	// show the addresses we found on the bus
	Serial.print("Device Address: ");
	printAddress(d);
	Serial.println();

	// set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
	sensors.setResolution(d, 9);

	Serial.print("Device Resolution: ");
	Serial.print(sensors.getResolution(d), DEC);
	Serial.println();
}


void setup() {

  Serial.begin(115200);

  // load device addresses from flash
  memcpy_P(one, mbr_in, devaddr_len);
  memcpy_P(two, mbr_out, devaddr_len);
  memcpy_P(three, valve_inb, devaddr_len);
  memcpy_P(four, valve_insys, devaddr_len);
  memcpy_P(five, valve_out, devaddr_len);

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

  sensors.begin();

  // setup devices
  setupDevice(one);
  setupDevice(two);
	setupDevice(three);
	setupDevice(four);
	setupDevice(five);
}

void processTemp(DeviceAddress d)
{
    WiFiClient client;
    HTTPClient http;
    
    // It responds almost immediately. Let's print out the data
    float tempC = sensors.getTempC(d);
    float tempF = sensors.toFahrenheit(tempC);
    printTemperature(d, tempC, tempF); // Use a simple function to print out the data
    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, "http://192.168.0.174:8000/api/heating/sensors/MBR-IN/data")) 
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
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("Done");

    processTemp(one);
    processTemp(two);
    processTemp(three);
    processTemp(four);
    processTemp(five);
  }

  delay(10000);
}
