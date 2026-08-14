#pragma once

#include <cstddef>

namespace services::adsb {

struct Aircraft {
  float lat;
  float lon;
  float nose_deg;
  float track_deg;
  float gs_knots;
  char callsign[9];
};

constexpr size_t kMaxAircraft = 64;
constexpr size_t kSourceNameMaxLen = 12;
constexpr size_t kEndpointUrlMaxLen = 128;
constexpr size_t kApiKeyMaxLen = 96;

size_t aircraftCount();
const Aircraft* aircraftList();

void settingsInit();
const char* sourceName();
const char* tar1090Url();
const char* adsbExchangeApiKey();
void saveSettingsFromPortal(const char* source, const char* tar1090_url,
                            const char* adsbx_key);

/** Hook invoked during long HTTP I/O (e.g. wifiLoop). Optional. */
using PollFn = void (*)();
void setPollFn(PollFn fn);

/** Fetch aircraft within fetch_radius_km of center_lat/lon. */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km);

}  // namespace services::adsb
