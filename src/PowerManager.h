#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Wire.h>
#include <XPowersLib.h>

class PowerManager {
private:
  XPowersAXP2101 PMU;

public:
  bool begin();
};

#endif