# Plane Radar

Firmware for the **Waveshare ESP32-S3-Touch-LCD-2.1** with a 2.1 inch
480x480 ST7701 RGB display. It shows a circular ADS-B radar around your
configured location, with WiFiManager for first-time setup.
Specifically this model: https://www.amazon.com/dp/B0DDPQSKJD?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_2
## What it does

1. **Wi-Fi setup** - captive portal on AP `PlaneRadar-Setup`
2. **Radar** - live aircraft from a local TAR1090/readsb receiver or ADS-B Exchange on a sonar-style grid

After Wi-Fi is saved, the device reconnects automatically. The radar runs in
the main loop with periodic ADS-B updates.

## Controls

The app uses the board's physical **BOOT** button on GPIO0 and the CST820
touch panel.

| Action | Effect |
|--------|--------|
| BOOT short tap | Cycle range preset (5 -> 10 -> 15 -> 25 -> 40 -> 50 km); saved to flash |
| BOOT hold 3 s | Clear Wi-Fi, location, and units; reboot into setup portal |
| Touch aircraft / rim dot | Open aircraft detail screen |
| Touch detail screen | Return to radar |

During setup you can also hold BOOT at power-on to force a credential reset.

## Wi-Fi setup portal

**First-time setup** (no saved Wi-Fi):

1. Connect to `PlaneRadar-Setup`
2. Open `http://plane-radar.local` (preferred) or `http://192.168.4.1`
3. Set home Wi-Fi, then save

**Reconfigure anytime** (after the device is on your network):

1. Open `http://plane-radar.local` or `http://<device-ip>`
2. Change Wi-Fi, ADS-B source, location, units, or runway overlay; save

The same portal runs on the setup AP and on the device's LAN IP while connected
to Wi-Fi. The mDNS hostname is `plane-radar`.

**Custom fields** (stored in NVS):

| Field | Purpose |
|-------|---------|
| Latitude / Longitude | Radar center and ADS-B query position |
| ADS-B source | `tar1090` for a local receiver, or `adsbx` for ADS-B Exchange |
| TAR1090 aircraft.json URL | Local endpoint such as `http://192.168.1.50/tar1090/data/aircraft.json` |
| ADS-B Exchange RapidAPI key | Required only when ADS-B source is `adsbx` |
| Display distances in miles | Ring scale label in mi instead of km |
| Show airport runways | Major-airport runway overlay on the radar |

### ADS-B Sources

For local ADS-B, run readsb/dump1090 with TAR1090 and use the HTTP
`aircraft.json` endpoint. Include `http://`; the firmware will also add it
automatically if you enter a bare LAN address. Common examples:

```text
http://192.168.1.50/tar1090/data/aircraft.json
http://192.168.1.50/data/aircraft.json
```

For ADS-B Exchange, set source to `adsbx` and enter a RapidAPI key. The
firmware calls the ADS-B Exchange `/v2/lat/.../lon/.../dist/.../` endpoint and
adds the required RapidAPI headers.

If the ADS-B source is offline, the firmware logs the HTTP error and backs off
before retrying so the radar sweep can continue animating.

## Radar display

### Grid

- Dark blue background, subdued green rings and crosshairs
- Looping green radar sweep behind aircraft
- White N / S / E / W at the bezel
- Range label on the east spoke
- White center dot

Layout and colors: `include/ui/radar_theme.h`.

### Range presets

| Ring 3 label | Outer radius (aircraft scale) |
|--------------|-------------------------------|
| 5 km / 3 mi | ~6.7 km |
| 10 km / 6 mi | ~13.3 km (default) |
| 15 km / 9 mi | ~20 km |
| 25 km / 16 mi | ~33.3 km |
| 40 km / 25 mi | ~53.3 km |
| 50 km / 31 mi | ~66.7 km |

Preset and miles/km choice persist across reboot (`planeradar` NVS namespace).

### Runways

- Major airports from OurAirports (`large_airport`)
- Teal runway lines with one ICAO label per airport
- Toggle in the Wi-Fi setup portal
- Update the embedded list: `python3 scripts/build_large_airports.py`

### Aircraft

- Inside the outer ring: red heading triangle, magenta speed vector, and callsign tags
- Outside the ring: small red dot on the screen rim at the correct bearing
- Callsign tags are placed toward the center to reduce edge clipping
- Tap an aircraft or rim dot to show callsign, heading, and speed

## Hardware

This port targets the Waveshare ESP32-S3-Touch-LCD-2.1:

| Area | Value |
|------|-------|
| MCU | ESP32-S3R8 |
| Flash / PSRAM | 16 MB flash / 8 MB OPI PSRAM |
| LCD | ST7701 RGB, 480x480, RGB565 |
| Touch | CST820 on I2C, used for aircraft selection |
| I2C | SCL GPIO7, SDA GPIO15 |
| Backlight | GPIO6 PWM |
| BOOT control | GPIO0 active LOW |

The ST7701 reset and chip-select lines are routed through the onboard TCA9554
I/O expander:

| Signal | Pin |
|--------|-----|
| LCD_RST | EXIO1 |
| TP_RST | EXIO2 |
| LCD_CS | EXIO3 |
| SD_CS | EXIO4 |
| Buzzer | EXIO8 |

RGB panel timing and pins are configured in `include/config.h` and applied by
`include/hardware/lgfx_config.hpp`.

## Configuration

Edit `include/config.h` for hardware and behavior:

| Area | Keys / notes |
|------|--------------|
| Portal | `kPortalApName`, `kPortalIp`, `kPortalHostname`, `kPortalHostUrl` |
| Wi-Fi timing | connect attempts, reconnect grace, portal timeout |
| BOOT | `kBootPin`, `kBootResetHoldMs`, `kBootTapMinMs` |
| Display / touch | ST7701 SPI/RGB pins, CST820 touch, timing, I2C, TCA9554, backlight |
| Default location | `kDefaultRadarLat`, `kDefaultRadarLon` |
| ADS-B | `kAdsbDefaultSource`, `kAdsbDefaultTar1090Url`, `kAdsbExchangeRapidApiKey`, `kAdsbFetchIntervalMs`, `kAdsbShowGroundAircraft` |

Range presets: `include/ui/radar_range.h` (`kRangePresets`).

## Project layout

```text
include/
  config.h
  hardware/
    lgfx_config.hpp
    display.h
    display_font.h
    touch.h
  data/
    large_airports.h
  ui/
    radar_theme.h
    radar_range.h
    radar_display.h
    runway_overlay.h
    status_screens.h
  services/
    wifi_setup.h
    radar_location.h
    adsb_client.h
data/
  ui_font.vlw
scripts/
  build_large_airports.py
  merge_firmware.py
  merge-firmware.sh
src/
  main.cpp
  data/
  hardware/
  ui/
  services/
```

## Build

```bash
pio run -e s3touch
pio run -t upload -e s3touch
pio device monitor
```

- PlatformIO env: `s3touch`
- Serial: 115200 baud
- Flash: ESP32-S3, 16 MB

### Web-flashable release image

Single `.bin` for [esptool-js](https://espressif.github.io/esptool-js/) and
similar tools, flashed at offset `0x0`:

```bash
chmod +x scripts/merge-firmware.sh
./scripts/merge-firmware.sh
```

Writes `release/plane-radar-merged.bin`. Skip rebuild if firmware is already
built:

```bash
./scripts/merge-firmware.sh --no-build
```

Or via PlatformIO only:

```bash
pio run -e s3touch
pio run -t merge -e s3touch
```

Put the board in download mode if needed: hold **BOOT**, tap **RESET**, then
release BOOT.

## CI and releases

| Workflow | When | Output |
|----------|------|--------|
| Build | Push / PR to `main` | Artifact `plane-radar-s3touch` |
| Release | Git tag `v*` | GitHub Release asset `plane-radar-<version>.bin` |

To ship a version users can download:

```bash
git tag v1.0.0
git push origin v1.0.0
```

## Dependencies

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
