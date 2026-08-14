#include "services/adsb_client.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "config.h"

namespace services::adsb {

namespace {

constexpr char kSourceTar1090[] = "tar1090";
constexpr char kSourceAdsbExchange[] = "adsbx";
constexpr char kSourceAdsbFi[] = "adsbfi";
constexpr char kAdsbFiApiBase[] = "https://opendata.adsb.fi/api/v3/lat/";
constexpr char kPrefsNamespace[] = "adsb";
constexpr char kPrefsSourceKey[] = "source";
constexpr char kPrefsTar1090UrlKey[] = "tar1090";
constexpr char kPrefsAdsbxKeyKey[] = "adsbxKey";
constexpr float kKmPerNm = 1.852f;
constexpr float kKmPerDeg = 111.0f;
constexpr int kConnectAttemptMs = 200;
constexpr unsigned long kTar1090RequestTimeoutMs = 6000;
constexpr unsigned long kAdsbxRequestTimeoutMs = 10000;

Aircraft s_aircraft[kMaxAircraft];
size_t s_aircraft_count = 0;
PollFn s_poll_fn = nullptr;
char s_source[kSourceNameMaxLen] = "";
char s_tar1090_url[kEndpointUrlMaxLen] = "";
char s_adsbx_key[kApiKeyMaxLen] = "";

void copyTrimmed(char* dst, size_t len, const char* src) {
  if (len == 0) {
    return;
  }
  dst[0] = '\0';
  if (src == nullptr) {
    return;
  }
  while (*src == ' ' || *src == '\t') {
    ++src;
  }
  size_t n = strnlen(src, len - 1);
  while (n > 0 && (src[n - 1] == ' ' || src[n - 1] == '\t' ||
                   src[n - 1] == '\r' || src[n - 1] == '\n')) {
    --n;
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

String normalizeHttpUrl(const char* src) {
  char trimmed[kEndpointUrlMaxLen];
  copyTrimmed(trimmed, sizeof(trimmed), src);
  String url(trimmed);
  if (url.length() > 0 && !url.startsWith("http://") &&
      !url.startsWith("https://")) {
    url = "http://" + url;
  }
  return url;
}

const char* normalizedSource(const char* source) {
  if (source == nullptr) {
    return kSourceAdsbFi;
  }
  if (strcasecmp(source, "adsbx") == 0 ||
      strcasecmp(source, "adsbexchange") == 0 ||
      strcasecmp(source, "ads-b exchange") == 0) {
    return kSourceAdsbExchange;
  }
  if (strcasecmp(source, "tar1090") == 0) {
    return kSourceTar1090;
  }
  if (strcasecmp(source, "adsbfi") == 0 ||
      strcasecmp(source, "adsb.fi") == 0 ||
      strcasecmp(source, "adsb-fi") == 0) {
    return kSourceAdsbFi;
  }
  return kSourceAdsbFi;
}

bool usingAdsbExchange() {
  return strcmp(s_source, kSourceAdsbExchange) == 0;
}

bool usingAdsbFi() { return strcmp(s_source, kSourceAdsbFi) == 0; }

void setDefaultSettings() {
  copyTrimmed(s_source, sizeof(s_source),
              normalizedSource(config::kAdsbDefaultSource));
  copyTrimmed(s_tar1090_url, sizeof(s_tar1090_url),
              config::kAdsbDefaultTar1090Url);
  copyTrimmed(s_adsbx_key, sizeof(s_adsbx_key),
              config::kAdsbExchangeRapidApiKey);
}

void pollNetwork() {
  if (s_poll_fn != nullptr) {
    s_poll_fn();
  }
}

int performGetWithPoll(HTTPClient& http, unsigned long timeout_ms) {
  http.setConnectTimeout(kConnectAttemptMs);
  const unsigned long deadline = millis() + timeout_ms;
  while (millis() < deadline) {
    pollNetwork();
    const int code = http.GET();
    if (code > 0) {
      return code;
    }
    if (code != HTTPC_ERROR_CONNECTION_REFUSED &&
        code != HTTPC_ERROR_NOT_CONNECTED) {
      return code;
    }
    delay(5);
  }
  return HTTPC_ERROR_READ_TIMEOUT;
}

bool jsonPayloadLooksComplete(const String& payload) {
  bool in_string = false;
  bool escaped = false;
  bool saw_open = false;
  int object_depth = 0;
  int array_depth = 0;

  for (size_t i = 0; i < payload.length(); ++i) {
    const char c = payload[i];

    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }

    if (c == '"') {
      in_string = true;
      continue;
    }
    if (c == '{') {
      ++object_depth;
      saw_open = true;
      continue;
    }
    if (c == '}') {
      --object_depth;
      if (object_depth < 0) {
        return false;
      }
      continue;
    }
    if (c == '[') {
      ++array_depth;
      saw_open = true;
      continue;
    }
    if (c == ']') {
      --array_depth;
      if (array_depth < 0) {
        return false;
      }
    }
  }

  return saw_open && !in_string && object_depth == 0 && array_depth == 0;
}

bool readResponseBodyWithPoll(HTTPClient& http, String& payload,
                              unsigned long timeout_ms) {
  WiFiClient* stream = http.getStreamPtr();
  if (stream == nullptr) {
    return false;
  }

  const int content_length = http.getSize();
  if (content_length > 0) {
    payload.reserve(static_cast<unsigned>(content_length + 1));
  }

  uint8_t buffer[512];
  const unsigned long deadline = millis() + timeout_ms;
  bool complete = false;
  while (millis() < deadline) {
    pollNetwork();
    const int available = stream->available();
    if (available > 0) {
      const int to_read =
          available > static_cast<int>(sizeof(buffer)) ? static_cast<int>(sizeof(buffer))
                                                       : available;
      const int read_bytes = stream->readBytes(buffer, to_read);
      if (read_bytes > 0) {
        payload.concat(reinterpret_cast<const char*>(buffer),
                       static_cast<unsigned>(read_bytes));
      }
    }
    if (content_length > 0 &&
        static_cast<int>(payload.length()) >= content_length) {
      complete = true;
      break;
    }
    if (!http.connected() && stream->available() <= 0) {
      complete = content_length <= 0;
      break;
    }
    delay(1);
  }

  if (payload.length() == 0) {
    return false;
  }

  if (!complete && content_length > 0) {
    Serial.printf("adsb: incomplete response (%u/%d bytes)\n",
                  static_cast<unsigned>(payload.length()), content_length);
    return false;
  }

  if (!jsonPayloadLooksComplete(payload)) {
    Serial.printf("adsb: incomplete JSON response (%u bytes)\n",
                  static_cast<unsigned>(payload.length()));
    return false;
  }

  return true;
}

float kmToNauticalMiles(float km) { return km / kKmPerNm; }

bool readJsonFloat(const JsonObject& obj, const char* key, float* out) {
  if (obj[key].is<float>() || obj[key].is<double>() || obj[key].is<int>()) {
    *out = obj[key].as<float>();
    return true;
  }
  return false;
}

float distanceKm(double center_lat, double center_lon, float lat, float lon) {
  const float lat_rad = static_cast<float>(center_lat * 0.017453292519943295);
  const float dx_km =
      static_cast<float>(lon - center_lon) * kKmPerDeg * cosf(lat_rad);
  const float dy_km = static_cast<float>(lat - center_lat) * kKmPerDeg;
  return sqrtf(dx_km * dx_km + dy_km * dy_km);
}

float pickNoseHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickTrackHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickGroundSpeed(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "gs", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "tas", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "ias", &v)) {
    return v;
  }
  return 0.0f;
}

bool isOnGround(const JsonObject& plane) {
  if (!plane["alt_baro"].is<const char*>()) {
    return false;
  }
  return strcmp(plane["alt_baro"].as<const char*>(), "ground") == 0;
}

void copyJsonStringTrimmed(const JsonObject& obj, const char* key, char* out,
                           size_t out_len) {
  out[0] = '\0';
  if (out_len == 0 || !obj[key].is<const char*>()) {
    return;
  }
  copyTrimmed(out, out_len, obj[key].as<const char*>());
}

void fillTagFields(Aircraft* ac, const JsonObject& plane) {
  copyJsonStringTrimmed(plane, "flight", ac->callsign, sizeof(ac->callsign));
  if (ac->callsign[0] == '\0') {
    copyJsonStringTrimmed(plane, "hex", ac->callsign, sizeof(ac->callsign));
  }
}

bool fetchHttpPayload(const String& url, bool add_adsbx_headers,
                      unsigned long timeout_ms,
                      String& payload) {
  WiFiClient client;
  WiFiClientSecure secure_client;
  HTTPClient http;

  bool begin_ok = false;
  if (url.startsWith("https://")) {
    secure_client.setInsecure();
    begin_ok = http.begin(secure_client, url);
  } else {
    begin_ok = http.begin(client, url);
  }

  if (!begin_ok) {
    Serial.println("adsb: http.begin failed");
    return false;
  }

  if (add_adsbx_headers) {
    http.addHeader("X-RapidAPI-Key", s_adsbx_key);
    http.addHeader("X-RapidAPI-Host", config::kAdsbExchangeRapidApiHost);
  }

  http.useHTTP10(true);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setReuse(false);
  http.setUserAgent("PlaneRadar/1.0");
  http.setTimeout(timeout_ms);
  const int code = performGetWithPoll(http, timeout_ms);
  if (code != HTTP_CODE_OK) {
    Serial.printf("adsb: HTTP %d (%s)\n", code, http.errorToString(code).c_str());
    http.end();
    return false;
  }

  if (!readResponseBodyWithPoll(http, payload, timeout_ms)) {
    Serial.println("adsb: empty response");
    http.end();
    return false;
  }
  http.end();
  return true;
}

bool parseAircraftArray(JsonArray planes, const char* source_label,
                        double center_lat, double center_lon,
                        float fetch_radius_km) {
  if (planes.isNull()) {
    s_aircraft_count = 0;
    Serial.printf("adsb: %s JSON missing aircraft array\n", source_label);
    return true;
  }

  size_t n = 0;
  size_t total = 0;
  size_t hidden_ground = 0;
  size_t missing_position = 0;
  size_t outside_radius = 0;
  size_t clipped = 0;

  for (JsonObject plane : planes) {
    ++total;
    float lat = 0.0f;
    float lon = 0.0f;
    if (!readJsonFloat(plane, "lat", &lat) ||
        !readJsonFloat(plane, "lon", &lon)) {
      ++missing_position;
      continue;
    }
    if (distanceKm(center_lat, center_lon, lat, lon) > fetch_radius_km) {
      ++outside_radius;
      continue;
    }
    if (isOnGround(plane) && !config::kAdsbShowGroundAircraft) {
      ++hidden_ground;
      continue;
    }
    if (n >= kMaxAircraft) {
      ++clipped;
      continue;
    }

    s_aircraft[n].lat = lat;
    s_aircraft[n].lon = lon;
    s_aircraft[n].nose_deg = pickNoseHeading(plane);
    s_aircraft[n].track_deg = pickTrackHeading(plane);
    s_aircraft[n].gs_knots = pickGroundSpeed(plane);
    fillTagFields(&s_aircraft[n], plane);
    ++n;
  }

  s_aircraft_count = n;
  Serial.printf(
      "adsb: %u aircraft (%u total, %u ground hidden, %u outside, %u no position, %u clipped)\n",
      static_cast<unsigned>(n), static_cast<unsigned>(total),
      static_cast<unsigned>(hidden_ground), static_cast<unsigned>(outside_radius),
      static_cast<unsigned>(missing_position), static_cast<unsigned>(clipped));
  return true;
}

bool parsePayload(const String& payload, const char* source_label,
                  const char* primary_array_key, const char* fallback_array_key,
                  double center_lat, double center_lon, float fetch_radius_km) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    if (err == DeserializationError::IncompleteInput) {
      Serial.printf("adsb: incomplete JSON response (%u bytes)\n",
                    static_cast<unsigned>(payload.length()));
      return false;
    }
    Serial.printf("adsb: JSON parse error: %s\n", err.c_str());
    return false;
  }

  JsonArray planes = doc[primary_array_key].as<JsonArray>();
  if (planes.isNull() && fallback_array_key != nullptr) {
    planes = doc[fallback_array_key].as<JsonArray>();
  }
  return parseAircraftArray(planes, source_label, center_lat, center_lon,
                            fetch_radius_km);
}

bool fetchTar1090(double center_lat, double center_lon, float fetch_radius_km) {
  if (s_tar1090_url[0] == '\0') {
    s_aircraft_count = 0;
    Serial.println("adsb: TAR1090 URL not set; configure it in the portal");
    return false;
  }

  const String url = normalizeHttpUrl(s_tar1090_url);
  Serial.printf("adsb: source tar1090 radius %.1f km url %s\n",
                fetch_radius_km, url.c_str());
  String payload;
  if (!fetchHttpPayload(url, false, kTar1090RequestTimeoutMs, payload)) {
    return false;
  }
  return parsePayload(payload, kSourceTar1090, "aircraft", "ac", center_lat,
                      center_lon, fetch_radius_km);
}

bool fetchAdsbExchange(double center_lat, double center_lon,
                       float fetch_radius_km) {
  if (s_adsbx_key[0] == '\0') {
    s_aircraft_count = 0;
    Serial.println("adsb: ADS-B Exchange API key not set; configure it in the portal");
    return false;
  }

  const float dist_nm = kmToNauticalMiles(fetch_radius_km);
  String url = "https://";
  url += config::kAdsbExchangeRapidApiHost;
  url += "/v2/lat/";
  url += String(center_lat, 6);
  url += "/lon/";
  url += String(center_lon, 6);
  url += "/dist/";
  url += String(dist_nm, 1);
  url += "/";

  Serial.printf("adsb: source adsbx lat %.6f lon %.6f radius %.1f km (%.1f nm)\n",
                center_lat, center_lon, fetch_radius_km, dist_nm);
  String payload;
  if (!fetchHttpPayload(url, true, kAdsbxRequestTimeoutMs, payload)) {
    return false;
  }
  return parsePayload(payload, kSourceAdsbExchange, "ac", "aircraft", center_lat,
                      center_lon, fetch_radius_km);
}

// Free public adsb.fi open-data network. No API key, plug-and-play.
// Same JSON schema as tar1090 (aircraft in the "ac" array).
bool fetchAdsbFi(double center_lat, double center_lon, float fetch_radius_km) {
  const float dist_nm = kmToNauticalMiles(fetch_radius_km);
  String url = kAdsbFiApiBase;
  url += String(center_lat, 6);
  url += "/lon/";
  url += String(center_lon, 6);
  url += "/dist/";
  url += String(dist_nm, 1);

  Serial.printf("adsb: source adsbfi lat %.6f lon %.6f radius %.1f km (%.1f nm)\n",
                center_lat, center_lon, fetch_radius_km, dist_nm);
  String payload;
  if (!fetchHttpPayload(url, false, kAdsbxRequestTimeoutMs, payload)) {
    return false;
  }
  return parsePayload(payload, kSourceAdsbFi, "ac", "aircraft", center_lat,
                      center_lon, fetch_radius_km);
}

}  // namespace

void settingsInit() {
  setDefaultSettings();

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    return;
  }
  const String source = prefs.getString(kPrefsSourceKey, s_source);
  const String tar1090 = prefs.getString(kPrefsTar1090UrlKey, s_tar1090_url);
  const String adsbx = prefs.getString(kPrefsAdsbxKeyKey, s_adsbx_key);
  prefs.end();

  copyTrimmed(s_source, sizeof(s_source), normalizedSource(source.c_str()));
  copyTrimmed(s_tar1090_url, sizeof(s_tar1090_url), tar1090.c_str());
  copyTrimmed(s_adsbx_key, sizeof(s_adsbx_key), adsbx.c_str());
}

const char* sourceName() { return s_source; }

const char* tar1090Url() { return s_tar1090_url; }

const char* adsbExchangeApiKey() { return s_adsbx_key; }

void saveSettingsFromPortal(const char* source, const char* tar1090_url,
                            const char* adsbx_key) {
  copyTrimmed(s_source, sizeof(s_source), normalizedSource(source));
  copyTrimmed(s_tar1090_url, sizeof(s_tar1090_url), tar1090_url);
  copyTrimmed(s_adsbx_key, sizeof(s_adsbx_key), adsbx_key);

  Preferences prefs;
  if (prefs.begin(kPrefsNamespace, false)) {
    prefs.putString(kPrefsSourceKey, s_source);
    prefs.putString(kPrefsTar1090UrlKey, s_tar1090_url);
    prefs.putString(kPrefsAdsbxKeyKey, s_adsbx_key);
    prefs.end();
  }

  Serial.printf("ADS-B source: %s\n", s_source);
  if (s_tar1090_url[0] != '\0') {
    Serial.printf("TAR1090 URL: %s\n", s_tar1090_url);
  }
  Serial.printf("ADS-B Exchange key: %s\n",
                s_adsbx_key[0] == '\0' ? "not set" : "set");
}

void setPollFn(PollFn fn) { s_poll_fn = fn; }

size_t aircraftCount() { return s_aircraft_count; }

const Aircraft* aircraftList() { return s_aircraft; }

bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km) {
  if (usingAdsbFi()) {
    return fetchAdsbFi(center_lat, center_lon, fetch_radius_km);
  }
  if (usingAdsbExchange()) {
    return fetchAdsbExchange(center_lat, center_lon, fetch_radius_km);
  }
  return fetchTar1090(center_lat, center_lon, fetch_radius_km);
}

}  // namespace services::adsb
