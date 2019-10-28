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


ESP8266WiFiMulti WiFiMulti;

#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress one = { 0x28, 0x5D, 0x43, 0x45, 0x92, 0x13, 0x02, 0xDE };
DeviceAddress two = { 0x28, 0x71, 0x3E, 0x45, 0x92, 0x0E, 0x02, 0xF1 };
DeviceAddress three = { 0x28, 0xBE, 0x1A, 0x45, 0x92, 0x16, 0x02, 0x37 };
DeviceAddress four = { 0x28, 0x96, 0x47, 0x45, 0x92, 0x13, 0x02, 0xA6 };
DeviceAddress five = { 0x28, 0xD2, 0x45, 0x45, 0x92, 0x13, 0x02, 0x8C };


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

  sensors.begin();
  
  // show the addresses we found on the bus
  Serial.print("Device Address: ");
  printAddress(one);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(one, 9);
 
  Serial.print("Device Resolution: ");
  Serial.print(sensors.getResolution(one), DEC); 
  Serial.println();

   // show the addresses we found on the bus
  Serial.print("Device Address: ");
  printAddress(two);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(two, 9);
 
  Serial.print("Device Resolution: ");
  Serial.print(sensors.getResolution(two), DEC); 
  Serial.println();
  
  Serial.print("Device Address: ");
  printAddress(three);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(three, 9);
 
  Serial.print("Device Resolution: ");
  Serial.print(sensors.getResolution(three), DEC); 
  Serial.println();

  Serial.print("Device Address: ");
  printAddress(four);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(four, 9);
 
  Serial.print("Device Resolution: ");
  Serial.print(sensors.getResolution(four), DEC); 
  Serial.println();

  Serial.print("Device Address: ");
  printAddress(five);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(five, 9);
 
  Serial.print("Device Resolution: ");
  Serial.print(sensors.getResolution(five), DEC); 
  Serial.println();
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
