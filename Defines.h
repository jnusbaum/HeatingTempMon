#ifndef Defines_h
#define Defines_h

#define DEVICENAME "CLOSET"
#define MQTTHOST "192.168.0.134"

// temp check interval                                                          
#define PERIOD 10000

#define BASETOPIC "sorrelhills"
#define TEMPTOPIC BASETOPIC "/temperature/"
#define RELAYTOPIC BASETOPIC "/relay/"
#define STATUSTOPIC BASETOPIC "/device/status/" DEVICENAME
#define CONFIGREQUESTTOPIC BASETOPIC "/device/config-request/" DEVICENAME
#define CONFIGRECEIVETOPIC BASETOPIC "/device/config/" DEVICENAME

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

#endif
