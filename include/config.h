#pragma once

#include <cstdint>

#include <driver/gpio.h>

namespace config {

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "PlaneRadar-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local" suffix); browser: http://plane-radar.local */
constexpr char kPortalHostname[] = "plane-radar";
constexpr char kPortalHostUrl[] = "plane-radar.local";

/** Per-attempt STA connect wait (ms); retried kWifiConnectAttempts times. */
constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
constexpr unsigned long kWifiConnectingFrameMs = 50;
/** Wait after disconnect before reconnecting (avoids portal on brief drops). */
constexpr unsigned long kWifiDownGraceMs = 4000;
/** Minimum interval between background reconnect tries. */
constexpr unsigned long kWifiReconnectIntervalMs = 15000;

// --- BOOT button (Waveshare ESP32-S3-Touch-LCD-2.1, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_0;
constexpr unsigned long kBootResetHoldMs = 3000UL;
/** Ignore BOOT taps shorter than this (debounce). */
constexpr unsigned long kBootTapMinMs = 40UL;

// --- Display: Waveshare 2.1" ST7701 RGB 480x480 ---
constexpr int kDisplayWidth = 480;
constexpr int kDisplayHeight = 480;

// ST7701 command/control SPI.
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_2;  // LCD_SCL
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_1;  // LCD_SDA

// ST7701 RGB data/timing pins from the Waveshare hardware table.
constexpr gpio_num_t kDisplayPinHsync = GPIO_NUM_38;
constexpr gpio_num_t kDisplayPinVsync = GPIO_NUM_39;
constexpr gpio_num_t kDisplayPinDe = GPIO_NUM_40;
constexpr gpio_num_t kDisplayPinPclk = GPIO_NUM_41;
constexpr gpio_num_t kDisplayPinD0 = GPIO_NUM_5;
constexpr gpio_num_t kDisplayPinD1 = GPIO_NUM_45;
constexpr gpio_num_t kDisplayPinD2 = GPIO_NUM_48;
constexpr gpio_num_t kDisplayPinD3 = GPIO_NUM_47;
constexpr gpio_num_t kDisplayPinD4 = GPIO_NUM_21;
constexpr gpio_num_t kDisplayPinD5 = GPIO_NUM_14;
constexpr gpio_num_t kDisplayPinD6 = GPIO_NUM_13;
constexpr gpio_num_t kDisplayPinD7 = GPIO_NUM_12;
constexpr gpio_num_t kDisplayPinD8 = GPIO_NUM_11;
constexpr gpio_num_t kDisplayPinD9 = GPIO_NUM_10;
constexpr gpio_num_t kDisplayPinD10 = GPIO_NUM_9;
constexpr gpio_num_t kDisplayPinD11 = GPIO_NUM_46;
constexpr gpio_num_t kDisplayPinD12 = GPIO_NUM_3;
constexpr gpio_num_t kDisplayPinD13 = GPIO_NUM_8;
constexpr gpio_num_t kDisplayPinD14 = GPIO_NUM_18;
constexpr gpio_num_t kDisplayPinD15 = GPIO_NUM_17;

constexpr uint32_t kDisplayRgbPclkHz = 16000000;
constexpr int kDisplayHsyncPulseWidth = 8;
constexpr int kDisplayHsyncBackPorch = 10;
constexpr int kDisplayHsyncFrontPorch = 50;
constexpr int kDisplayVsyncPulseWidth = 3;
constexpr int kDisplayVsyncBackPorch = 8;
constexpr int kDisplayVsyncFrontPorch = 8;
constexpr bool kDisplayHsyncPolarity = false;
constexpr bool kDisplayVsyncPolarity = false;
constexpr bool kDisplayPclkActiveNeg = false;
constexpr bool kDisplayPclkIdleHigh = false;
constexpr bool kDisplayDeIdleHigh = false;
constexpr bool kDisplayInvert = false;
constexpr bool kDisplayRgbOrder = false;

// Shared I2C bus for the TCA9554 expander, CST820 touch, RTC, and IMU.
constexpr gpio_num_t kI2cSclPin = GPIO_NUM_7;
constexpr gpio_num_t kI2cSdaPin = GPIO_NUM_15;
constexpr uint32_t kI2cFreqHz = 400000;

// CST820 capacitive touch controller.
constexpr gpio_num_t kTouchIntPin = GPIO_NUM_16;
constexpr uint8_t kTouchI2cAddress = 0x15;
constexpr uint8_t kTouchReadReg = 0x01;
constexpr uint8_t kTouchDisableAutoSleepReg = 0xFE;

constexpr uint8_t kTca9554Address = 0x20;
constexpr uint8_t kTca9554OutputReg = 0x01;
constexpr uint8_t kTca9554ConfigReg = 0x03;
constexpr uint8_t kTca9554LcdResetPin = 1;    // EXIO1
constexpr uint8_t kTca9554TouchResetPin = 2;  // EXIO2
constexpr uint8_t kTca9554LcdCsPin = 3;       // EXIO3
constexpr uint8_t kTca9554SdCsPin = 4;        // EXIO4
constexpr uint8_t kTca9554BuzzerPin = 8;      // EXIO8

constexpr gpio_num_t kDisplayBacklightPin = GPIO_NUM_6;
constexpr uint32_t kDisplayBacklightPwmHz = 20000;

// --- Radar center defaults (overridden via WiFi setup portal) ---
// Default set to Ruegen/Glowe; change it in the portal for your location.
constexpr double kDefaultRadarLat = 54.5700;
constexpr double kDefaultRadarLon = 13.4400;

/**
 * ADS-B source defaults. These can be overridden from the Wi-Fi portal.
 *
 * kAdsbDefaultSource: "adsbfi" (free public network, no key, plug-and-play),
 *   "tar1090" (your own local receiver), or "adsbx" (ADS-B Exchange, needs key).
 * kAdsbDefaultTar1090Url example:
 *   "http://192.168.1.50/tar1090/data/aircraft.json"
 */
constexpr char kAdsbDefaultSource[] = "adsbfi";
constexpr char kAdsbDefaultTar1090Url[] = "";
constexpr char kAdsbExchangeRapidApiHost[] = "adsbexchange-com1.p.rapidapi.com";
constexpr char kAdsbExchangeRapidApiKey[] = "";
constexpr unsigned long kAdsbFetchIntervalMs = 3000;
constexpr unsigned long kAdsbRetryAfterErrorMs = 30000;
/** Legacy scale unused; fetch uses radar::fetchRadiusKm() to screen edge. */
constexpr float kAdsbFetchRadiusScale = 1.0f;
/** false = hide aircraft with alt_baro "ground"; true = show them too. */
constexpr bool kAdsbShowGroundAircraft = true;

// --- UI colors (RGB565) - status screens ---
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;

}  // namespace config
