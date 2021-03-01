#ifndef TempSensor_h
#define TempSensor_h

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

#include "Defines.h"
#include "DeviceAddresses.h"

#define MAXSENSORS 16


class TempSensor {
  public:
    char devname[64];
    DeviceAddress devaddr;

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
};


class SensorBus {
  public:
    int pin;
    OneWire *wire;
    DallasTemperature bus;
    int numsensors;
    TempSensor sensors[MAXSENSORS];;

    SensorBus() {
      pin = 0;
      wire = nullptr;
      numsensors = 0;
    }

    void initialize(const int i_pin, const int i_numsensors) {
      pin = i_pin;
      numsensors = i_numsensors;
      DEBUG_PRINTF("initing onewire on pin: %d\n", pin);
      wire = new OneWire(pin);
      DEBUG_PRINTLN("initing dallas temp");
      bus.setOneWire(wire);
    }

    void initsensor(const int i_sensoridx, const char *i_devname, const char *i_daddress) {
      sensors[i_sensoridx].initialize(i_devname, i_daddress);
    }


    void begin() {
      bus.begin();
      // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
      bus.setResolution(9);
      yield();
    }

    void requestTemps() {
      bus.requestTemperatures();
    }

    float getTempC(int y) {
      float tempC = bus.getTempC(sensors[y].devaddr);
      return tempC;
    }

    float getTempF(int y) {
      float tempC = getTempC(y);
      float tempF = bus.toFahrenheit(tempC);
      return tempF;
    }


    void processTemps() {
      for (int y = 0; y < numsensors; ++y)
      {
        float tempC = getTempC(y);
        float tempF = getTempF(y);
      #ifdef DEBUG
        printTemperature(sensors[y].devname, tempC, tempF); // Use a simple function to print out the data
      #endif
      }
    }

    void shutdown() {
      numsensors = 0;
      delete wire;
      wire = nullptr;
    }

    ~SensorBus() {
      shutdown();
    }

};

#endif
