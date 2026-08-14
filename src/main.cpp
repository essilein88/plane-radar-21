/**
 * Plane Radar - WiFi setup, then radar UI on the Waveshare 2.1" display.
 */

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "hardware/display.h"
#include "hardware/touch.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "services/wifi_setup.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"
#include "ui/status_screens.h"

namespace {

bool g_radar_visible = false;
unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;
unsigned long g_next_adsb_fetch_ms = 0;

void showRadarIfConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    g_radar_visible = false;
    return;
  }
  ui::radarDisplayDraw();
  g_next_adsb_fetch_ms = millis() + config::kAdsbFetchIntervalMs;
  g_radar_visible = true;
}

void onRangeTap() {
  ui::radar::rangeNext();
  char range_label[12];
  ui::radar::formatCurrentRing3Label(range_label, sizeof(range_label));
  Serial.printf("Range: %s (outer ~%.0f km)\n", range_label,
                ui::radar::rangeCurrent().outer_km);

  if (g_radar_visible && WiFi.status() == WL_CONNECTED) {
    ui::radarDisplayDraw();
  }
  g_next_adsb_fetch_ms = 0;
}

void handleBootButton() {
  bootButtonPollLongPress();
  if (bootButtonConsumeTap()) {
    onRangeTap();
  }
}

void handleTouch() {
  hardware::touch::Point point{};
  if (hardware::touch::readTap(&point)) {
    ui::radarDisplayHandleTouch(point.x, point.y);
  }
}

bool fetchAndDrawAircraft() {
  const float fetch_km = ui::radar::fetchRadiusKm();
  if (!services::adsb::fetchUpdate(services::location::lat(),
                                   services::location::lon(), fetch_km)) {
    handleBootButton();
    handleTouch();
    return false;
  }
  ui::radarDisplayRefreshAircraft();
  handleBootButton();
  handleTouch();
  return true;
}

void pollDuringAdsbFetch() {
  wifiLoop();
  handleBootButton();
  handleTouch();
  if (g_radar_visible) {
    ui::radarDisplayAnimate();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Plane Radar");

  bootButtonInit();
  displayInit();
  hardware::touch::init();
  if (wifiShowsSetupScreenOnBoot()) {
    statusScreenPortal();
  }
  services::location::init();
  ui::radar::rangeInit();
  services::adsb::settingsInit();
  services::adsb::setPollFn(pollDuringAdsbFetch);

  if (wifiSetupConnect()) {
    showRadarIfConnected();
  }
}

void loop() {
  handleBootButton();
  handleTouch();
  wifiLoop();

  if (WiFi.status() != WL_CONNECTED) {
    if (g_radar_visible) {
      Serial.println("WiFi lost — will reconnect");
      g_radar_visible = false;
    }

    if (g_wifi_down_since == 0) {
      g_wifi_down_since = millis();
    }

    const unsigned long down_ms = millis() - g_wifi_down_since;
    if (down_ms >= config::kWifiDownGraceMs &&
        millis() - g_last_reconnect_ms >= config::kWifiReconnectIntervalMs) {
      g_last_reconnect_ms = millis();
      if (wifiReconnect()) {
        g_wifi_down_since = 0;
        showRadarIfConnected();
      }
    }
  } else {
    g_wifi_down_since = 0;
    if (!g_radar_visible) {
      showRadarIfConnected();
    } else if (g_next_adsb_fetch_ms == 0 ||
               static_cast<long>(millis() - g_next_adsb_fetch_ms) >= 0) {
      const bool fetch_ok = fetchAndDrawAircraft();
      const unsigned long wait_ms =
          fetch_ok ? config::kAdsbFetchIntervalMs : config::kAdsbRetryAfterErrorMs;
      g_next_adsb_fetch_ms = millis() + wait_ms;
      if (!fetch_ok) {
        Serial.printf("adsb: fetch failed; retry in %lu seconds\n", wait_ms / 1000UL);
      }
    } else {
      ui::radarDisplayAnimate();
    }
  }

  delay(10);
}
