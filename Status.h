#ifndef Status_h
#define Status_h

#include <MQTT.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include "Defines.h"
#include "Constants.h"


void publishStatus(MQTTClient &client, NTPClient &timeClient, const char *statusstr)
{
  StaticJsonDocument<128> doc;
  doc["device"] = DEVICENAME;
  doc["status"] = statusstr;
  doc["timestamp"] = timeClient.getEpochTime();;
  char buffer[128];
  int n = serializeJson(doc, buffer);
  DEBUG_PRINTF("[MQTT] PUBLISHing to %s\n", STATUSTOPIC);
  client.publish(STATUSTOPIC, buffer, n);
  DEBUG_PRINTLN("Published.");
}


void publishState(MQTTClient &client, NTPClient &timeClient, const char *relay, bool value)
{
  unsigned long etime = timeClient.getEpochTime();
  StaticJsonDocument<128> doc;
  doc["device"] = DEVICENAME;
  doc["relay"] = relay;
  doc["timestamp"] = etime;
  doc["value"] = value;
  // Generate the minified JSON and put it in buffer.
  char topic[256];
  strcpy(topic, RELAYTOPIC);
  strcat(topic, relay);
  char buffer[128];
  int n = serializeJson(doc, buffer);
  DEBUG_PRINTF("[MQTT] PUBLISHing to %s\n", topic);
  client.publish(topic, buffer, n);
}


void publishTemp(MQTTClient &client, NTPClient &timeClient, const char *sensor, float value)
{
  unsigned long etime = timeClient.getEpochTime();
  StaticJsonDocument<128> doc;
  doc["device"] = DEVICENAME;
  doc["sensor"] = sensor;
  doc["timestamp"] = etime;
  doc["value"] = value;
  // Generate the minified JSON and put it in buffer.
  char topic[256];
  strcpy(topic, TEMPTOPIC);
  strcat(topic, sensor);
  char buffer[128];
  int n = serializeJson(doc, buffer);
  DEBUG_PRINTF("[MQTT] PUBLISHing to %s\n", topic);
  client.publish(topic, buffer, n);
}

#endif
