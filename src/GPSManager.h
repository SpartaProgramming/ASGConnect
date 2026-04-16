#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <HardwareSerial.h>
#include <TinyGPS++.h>

class GPSManager {
private:
  HardwareSerial serial;
  TinyGPSPlus gps;

public:
  GPSManager();
  void begin();
  void update();

  double getLat();
  double getLon();
  bool hasFix();
};

#endif