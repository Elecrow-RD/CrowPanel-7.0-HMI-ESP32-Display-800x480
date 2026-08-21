#include "PCA9557.h"

#define PCA9557_INPUT_PORT    0x00
#define PCA9557_OUTPUT_PORT   0x01
#define PCA9557_POLARITY_INV  0x02
#define PCA9557_CONFIG        0x03

PCA9557::PCA9557(uint8_t addr) {
  _addr = addr;
  _config = 0xFF; // all input by default
  _output = 0x00;
}

void PCA9557::writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void PCA9557::reset() {
  // Reset to default: all inputs, output 0
  _config = 0xFF;
  _output = 0x00;
  writeReg(PCA9557_CONFIG, _config);
  writeReg(PCA9557_OUTPUT_PORT, _output);
}

void PCA9557::setMode(uint8_t mode) {
  // Set all 8 pins to same mode (Arduino compatible API)
  if (mode == IO_OUTPUT) {
    _config = 0x00;
  } else {
    _config = 0xFF;
  }
  writeReg(PCA9557_CONFIG, _config);
}

void PCA9557::setMode(uint8_t pin, uint8_t mode) {
  if (pin > 7) return;
  if (mode == IO_OUTPUT) {
    _config &= ~(1 << pin);
  } else {
    _config |= (1 << pin);
  }
  writeReg(PCA9557_CONFIG, _config);
}

void PCA9557::setState(uint8_t pin, uint8_t state) {
  if (pin > 7) return;
  if (state == IO_HIGH) {
    _output |= (1 << pin);
  } else {
    _output &= ~(1 << pin);
  }
  writeReg(PCA9557_OUTPUT_PORT, _output);
}