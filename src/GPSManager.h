#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include <TinyGPS++.h>

class GPSManager {
public:
  TinyGPSPlus gps;
  HardwareSerial serial;

  GPSManager();
  void begin();
  void update();
};

#endif