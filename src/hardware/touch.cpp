#include "hardware/touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "hardware/lgfx_config.hpp"

namespace hardware::touch {
namespace {

bool s_ready = false;
bool s_was_down = false;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(config::kTouchI2cAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(config::kTouchI2cAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t requested =
      Wire.requestFrom(config::kTouchI2cAddress, static_cast<uint8_t>(len));
  if (requested != len) {
    while (Wire.available() > 0) {
      Wire.read();
    }
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

uint16_t clampToDisplay(uint16_t v, int max_value) {
  if (v >= static_cast<uint16_t>(max_value)) {
    return static_cast<uint16_t>(max_value - 1);
  }
  return v;
}

}  // namespace

void init() {
  pinMode(static_cast<uint8_t>(config::kTouchIntPin), INPUT_PULLUP);

  waveshare_s3touch::tcaSet(config::kTca9554TouchResetPin, false);
  delay(10);
  waveshare_s3touch::tcaSet(config::kTca9554TouchResetPin, true);
  delay(50);

  s_ready = writeRegister(config::kTouchDisableAutoSleepReg, 0xFF);
  s_was_down = false;
  Serial.printf("touch: CST820 %s\n", s_ready ? "ready" : "not responding");
}

bool readTap(Point* out) {
  if (!s_ready || out == nullptr) {
    return false;
  }

  uint8_t buf[6] = {};
  if (!readRegisters(config::kTouchReadReg, buf, sizeof(buf))) {
    s_was_down = false;
    return false;
  }

  const uint8_t points = buf[1];
  const bool down = points > 0;
  if (!down) {
    s_was_down = false;
    return false;
  }

  const uint16_t raw_x =
      static_cast<uint16_t>(((buf[2] & 0x0F) << 8) | buf[3]);
  const uint16_t raw_y =
      static_cast<uint16_t>(((buf[4] & 0x0F) << 8) | buf[5]);

  if (s_was_down) {
    return false;
  }

  s_was_down = true;
  out->x = clampToDisplay(raw_x, config::kDisplayWidth);
  out->y = clampToDisplay(raw_y, config::kDisplayHeight);
  return true;
}

}  // namespace hardware::touch
