#ifndef _PCA9557_H_
#define _PCA9557_H_

#include "Arduino.h"
#include <Wire.h>

#define IO_OUTPUT 0
#define IO_INPUT  1
#define IO_LOW    0
#define IO_HIGH   1

class PCA9557 {
public:
  PCA9557(uint8_t addr = 0x18);
  void reset();
  void setMode(uint8_t mode);           // Set all pins mode at once (Arduino compatible)
  void setMode(uint8_t pin, uint8_t mode);
  void setState(uint8_t pin, uint8_t state);
private:
  uint8_t _addr;
  uint8_t _config;
  uint8_t _output;
  void writeReg(uint8_t reg, uint8_t val);
};

#endif