#include "hardware/display.h"

#include <Arduino.h>

#include "hardware/display_font.h"

LGFX tft;

void displayInit() {
  tft.init();
  tft.setColorDepth(16);
  tft.setRotation(0);
  tft.setBrightness(255);
  tft.setTextWrap(false);
  Serial.printf("Display: PSRAM %s, free heap %u, free PSRAM %u\n",
                psramFound() ? "detected" : "not detected",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getFreePsram()));
  displayFontInit();
}
