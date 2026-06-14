# Solar Ball — Directional Light Sensor for Solar Arrays

[![Tests](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/test.yml/badge.svg)](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/test.yml)
[![Firmware Build](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/firmware.yml/badge.svg)](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/firmware.yml)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.3.0-green)](firmware/main/config.h)

A self-contained, self-powered directional light sensor that tells solar arrays where the sun is.  
80 phototransistors on a 100mm sphere → weighted centroid → direction vector → 4G MQTT → array tracking.  
**BOM ~¥412 / ~$55. Direction accuracy 0.3–0.5° (calibrated).**

> 📖 **新手入门？** 直接看[操作手册 (中文)](docs/operation_manual.md) 或 [Operation Manual (English)](docs/operation_manual_en.md) — 从零件采购到日常运行的完整指南。

## Data Flow

```
  Sky light → 80×ALS-PT19 → 5×MUX → 2×ADS1115 ADC → ESP32
                                                         │
                                              ┌──────────┤
                                              ▼          ▼
                                     direction.c    sensor_diag.c
                                     (dx,dy,dz)    (faults,weather,confidence)
                                              │          │
                                              ▼          ▼
                                       display.c    mqtt_4g.c
                                       (MIP LCD)    (JSON via 4G)
                                                         │
                                              ┌──────────┤
                                              ▼          ▼
                                        MQTT Broker   mqtt_receiver.py
                                              │       (az,elev,panel angles)
                                              ▼
                                        Array Motor Controller
```

## Specifications

| Parameter | Value |
|-----------|-------|
| Sensors | 80 × ALS-PT19 phototransistors |
| Sphere | 100mm diameter, Fibonacci lattice |
| Direction accuracy | **0.3–0.5°** (auto-calibrated) |
| Update interval | 5 seconds |
| MCU | ESP32-WROOM-32 (ESP-IDF v5 / PlatformIO) |
| Communication | SIM7600G 4G Cat-4, MQTT QoS 1 |
| Display | Sharp Memory LCD 128×128 (MIP reflective, SPI, always-on) |
| Power | 5W solar panel + 18650 2000mAh |
| BOM cost | **~¥412 (~$55)** |
| Firmware | C, 12 modules, ~2800 lines |
| Tests | 8 suites, **419 assertions**, all passing |

## Features

| Category | Feature | Status |
|----------|---------|:------:|
| **Core** | 80-sensor Fibonacci sphere scan + 8x oversampling | ✅ |
| | Weighted centroid direction algorithm | ✅ |
| | Self-powered (solar + Li-ion, 3-day battery) | ✅ |
| | 4G MQTT publish every 5 seconds | ✅ |
| **Calibration** | Sun auto-calibration (one outdoor scan, no manual steps) | ✅ |
| | Manual flashlight calibration (80-step guided) | ✅ |
| | Baseline normalization (uniform light reference) | ✅ |
| | NVS persistent storage | ✅ |
| **Diagnostics** | Sensor fault detection (open/short/saturated) | ✅ |
| | Weather classification (clear / overcast / night) | ✅ |
| | Direction confidence scoring (0–255) | ✅ |
| | Remote diagnostic counters (scan/publish stats) | ✅ |
| **Display** | Sharp Memory LCD (MIP reflective, SPI, always-on image) | ✅ |
| | Shows direction, azimuth, elevation, SOC, RSSI, faults, confidence bar | ✅ |
| **Safety** | Overcast/night → auto-point zenith, low confidence | ✅ |
| | Low battery fault flag | ✅ |
| | Modem/MQTT failure flags in payload | ✅ |
| **Hardware** | Dust/sand protection design (replaceable PTFE sock) | ✅ |
| | Piezo vibration cleaning (optional) | ✅ |
| **Tooling** | PC calibration tool (serial UART, manual + auto modes) | ✅ |
| | MQTT receiver (subscribe → vector → panel angles) | ✅ |
| | Desktop simulator (full pipeline, noise modeling) | ✅ |
| | **Web configuration dashboard (Flask + SSE + Chart.js)** | ✅ |
| | **Remote configuration via MQTT (NVS persistent)** | ✅ |
| | **Calibration quality report (coverage, gaps, grade)** | ✅ |
| | **Config wizard (web-based config.h generator)** | ✅ |
| | **Fault root cause analysis (deep diagnostics)** | ✅ |
| | **Architecture documentation (docs/architecture.md)** | ✅ |
| | **Docker support (dashboard, MQTT broker, test runner)** | ✅ |
| | **Integration tests (28 tests, MQTT + OTA + pipeline)** | ✅ |
| | OTA firmware upgrade (MQTT + HTTP, serial UART) | ✅ |
| | CI/CD (GitHub Actions: Python tests + firmware build) | ✅ |

## Energy Efficiency Gain

Dual-axis tracking vs fixed-tilt panels:

| Region | Latitude | Annual Gain | Winter Gain |
|--------|:--------:|:-----------:|:-----------:|
| Beijing | 40°N | +35–40% | +50–100% |
| Shanghai | 31°N | +30–35% | +40–80% |
| Shenzhen | 22°N | +25–30% | +30–50% |

Solar Ball tracking accuracy of 0.3–0.5° adds <0.005% misalignment loss.

## Project Structure

```
solar-ball/
├── firmware/
│   ├── platformio.ini              PlatformIO + ESP-IDF build config
│   ├── CMakeLists.txt              Top-level CMake
│   ├── sdkconfig.defaults          Kconfig defaults (4MB flash, I2C legacy)
│   └── main/
│       ├── CMakeLists.txt          Component registration (12 source files)
│       ├── config.h                All pin mappings, broker, APN, timing
│       ├── sensor_positions.h      80 Fibonacci unit vectors (auto-generated)
│       ├── sensor_calib.h          Calibration data structures + state enum
│       ├── mqtt_protocol.h         MQTT packet struct + JSON serialize (v1.1)
│       ├── main.c                  Boot → scan → diag → compute → display → publish → sleep
│       ├── sensor_scan.h/.c        80-ch MUX + ADS1115 I2C driver + normalization
│       ├── sensor_diag.h/.c        Fault detection / weather / confidence
│       ├── direction.h/.c          Weighted centroid algorithm
│       ├── mqtt_4g.h/.c            SIM7600G AT commands (GPS, NTP, MQTT)
│       ├── power.h/.c              Battery ADC + SOC + deep sleep
│       ├── calibrate.h/.c          NVS + auto sun-cal + manual flashlight cal
│       ├── sun_calc.h/.c           NOAA solar ephemeris (static)
│       ├── display.h/.c            Sharp Memory LCD SPI driver (always-on MIP reflective)
│       ├── remote_diag.h/.c        Scan/publish counters
│       ├── ota.h/.c                OTA firmware upgrade (MQTT + HTTP, serial UART)
│       ├── remote_config.h/.c      Remote config via MQTT (NVS persistent)
│       └── scripts/
│           └── generate_sensor_coords.py  Fibonacci sphere → C header
├── tools/
│   ├── auto_calibrate.py           Calibration tool (--auto for sun, default for flashlight)
│   ├── ota_update.py               OTA firmware update (serial UART or MQTT trigger)
│   ├── mqtt_receiver.py            MQTT subscriber: (dx,dy,dz) → azimuth/elevation/panel angles
│   └── requirements.txt            pyserial, colorama, paho-mqtt
├── tests/
│   ├── run_all.py                  Test runner (8 suites)
│   ├── test_direction.py           38 assertions — algorithm correctness
│   ├── test_mqtt_protocol.py       25 assertions — JSON serialization
│   ├── test_power.py               20 assertions — battery SOC calculation
│   ├── test_calibration.py         246 assertions — channel mapping inversion
│   ├── test_weather.py             26 assertions — overcast/night/fault classification
│   ├── test_saturation.py          15 assertions — saturation detection
│   ├── test_remote_diag.py         20 assertions — diagnostic protocol
│   ├── test_autocal_validation.py  200-trial Monte Carlo — auto-cal accuracy
│   ├── test_autocal_e2e.py         GPS→ephemeris→calibrate→direction pipeline
│   └── simulate.py                 Desktop simulator (noise, accuracy sweep)
├── hardware/
│   ├── BOM.md                      ~¥412 bill of materials
│   ├── schematic.md                Full circuit + GPIO pin map
│   ├── ball_design.md              3D shell + sensor placement design
│   └── dust_protection.md          Replaceable PTFE sock + vibration cleaning
├── docs/
│   ├── operation_manual.md           **START HERE** — complete beginner guide (中文)
│   ├── operation_manual_en.md        Operation Manual (English)
│   ├── assembly_guide.md             Step-by-step build instructions
│   ├── calibration_procedure.md      Auto + manual calibration guide
│   └── architecture.md               Full system design and data flow
├── .github/workflows/
│   ├── test.yml                    Python tests on push
│   └── firmware.yml                PlatformIO build on push
├── web_dashboard/
│   ├── app.py                      Flask backend + MQTT + SSE + SQLite
│   ├── templates/index.html        Dashboard HTML
│   ├── static/dashboard.js         Real-time frontend logic
│   ├── Dockerfile                  Container for dashboard
│   ├── requirements.txt            flask, paho-mqtt
│   └── config.json                 Default settings
├── docker/
│   └── mosquitto.conf              Eclipse Mosquitto broker config
├── docker-compose.yml              Dashboard + MQTT broker + test runner
├── Dockerfile.test                 Container for automated tests
├── .dockerignore
├── CHANGELOG.md                    Full version history
├── CONTRIBUTING.md                 Development guide
├── LICENSE                         MIT
└── README.md
```

## Quick Start

```bash
# Generate sensor coordinates (if you need to regenerate sensor_positions.h)
# cd firmware/main/scripts && python generate_sensor_coords.py

# Build firmware
cd firmware && platformio run

# Flash to ESP32
platformio run --target upload

# Calibration with quality report
python tools/auto_calibrate.py COM3 --report

# OTA firmware update (serial UART)
python tools/ota_update.py COM3 firmware/.pio/build/esp32dev/firmware.bin

# OTA firmware update (MQTT trigger, remote)
python tools/ota_update.py --mqtt --ball ball-001 --url http://your-server.com/ota.bin

# Run test suite (418 assertions, zero dependencies beyond Python 3)
cd tests && python run_all.py

# Desktop simulation (no hardware needed)
python tests/simulate.py                # random sun positions
python tests/simulate.py --scan         # full sky sweep
python tests/simulate.py --noiseless    # ideal sensor accuracy test

# Docker (dashboard + MQTT broker + tests)
docker compose up -d                    # start dashboard at http://localhost:5000
docker compose --profile testing up tests  # run unit tests in container
docker compose --profile testing up integration-tests  # run integration tests
```

## Calibration

### Method 1: Sun Auto-Calibration (Recommended)

```bash
python tools/auto_calibrate.py COM3 --auto
```

Place ball outdoors under clear sky. One scan. Fully automatic:
GPS location → NTP time → solar ephemeris → scan → sort-and-map → NVS save.

### Method 2: Baseline Normalization

Enter calibration mode (hold BOOT button at power-on), then send:
```
CAL:BASELINE_START
```
Place ball in uniform diffuse light. 100 samples per sensor → stored as baseline reference.

### Method 3: Manual Flashlight

```bash
python tools/auto_calibrate.py COM3
```
80-step guided process with a bright flashlight.

## MQTT Protocol (v1.1)

Published every 5 seconds to `/solar/ball/{id}/direction`:

```json
{
  "id":    "ball-001",
  "ts":    1718000000,
  "dx":    0.3215,
  "dy":    -0.1478,
  "dz":    0.9352,
  "soc":   87,
  "rssi":  -89,
  "err":   0,
  "conf":  220,
  "flags": 15
}
```

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Ball identifier |
| `ts` | uint32 | Unix timestamp (UTC, timezone-corrected) |
| `dx,dy,dz` | float | Unit vector toward brightest sky (x=east, y=north, z=up) |
| `soc` | uint8 | Battery 0–100% |
| `rssi` | int16 | 4G signal (dBm, -999=unknown) |
| `err` | uint16 | Fault bitmask (see below) |
| `conf` | uint8 | Direction confidence 0–255 (255=perfect, <50=doubtful) |
| `flags` | uint8 | Status flags (direction valid, overcast, night, calibrated, baseline) |

### Fault Bitmask

| Bit | Value | Meaning |
|-----|-------|---------|
| 0 | 0x0001 | Sensor open circuit |
| 1 | 0x0002 | Sensor shorted |
| 2 | 0x0004 | Sensor saturated |
| 3 | 0x0008 | ADC I2C error |
| 4 | 0x0010 | 4G modem failure |
| 5 | 0x0020 | MQTT publish failure |
| 6 | 0x0040 | Low battery |
| 7 | 0x0080 | No calibration data |
| 8 | 0x0100 | Overcast sky (uniform illumination) |
| 9 | 0x0200 | Nighttime (no signal) |

### Command Topic (OTA)

The ball subscribes to `/solar/ball/{id}/cmd` for remote commands.

**Trigger OTA update:**

```json
{"cmd":"ota","url":"http://ota-server.example.com/solar-ball-v1.3.0.bin","version":"1.3.0"}
```

On receiving this, the ball downloads the firmware via the SIM7600G's HTTP client, writes it to the OTA partition, and reboots.

## Receiver

```bash
pip install -r tools/requirements.txt
python tools/mqtt_receiver.py                     # default broker, ball-001
python tools/mqtt_receiver.py --ball ball-002     # specific ball ID
python tools/mqtt_receiver.py --broker 192.168.1.1  # custom broker
```

Output:
```
[ball-001] 14:32:05 Sun: az= 169.2° elev=+73.0°  Panel: az= 169.2° tilt=17.0°  SOC=87% RSSI=-89dBm conf=220
  Faults: none  Flags: [DIR_OK CALIB ]  vector:OK
```

The receiver decodes all v1.1 fields: timestamp, faults, confidence, flags — and outputs
ready-to-use panel azimuth and tilt angles for motor controllers.

## Test Suite

```
$ python tests/run_all.py
============================================================
  test_direction.py         — 38 passed    algorithm correctness
  test_mqtt_protocol.py     — 25 passed    JSON serialization
  test_power.py             — 20 passed    battery SOC interpolation
  test_calibration.py       — 246 passed   channel mapping inversion
  test_weather.py           — 26 passed    overcast/night/fault classification
  test_saturation.py        — 15 passed    saturation threshold detection
  test_remote_diag.py       — 20 passed    diagnostic protocol
  test_integration.py       — 27 passed    E2E pipeline, MQTT, OTA, multi-ball, faults
============================================================
All tests passed!  (419 assertions, zero failures)
```

### Integration tests (28 tests, 1 skipped):
- Full sensor→direction→MQTT→receiver pipeline
- MQTT v1.1 protocol field validation
- OTA command protocol encode/decode
- Multi-ball scenario testing
- Fault detection pipeline
- SQLite history logging
- Live broker pub/sub (skipped without network)

Additional validation scripts:
- `test_autocal_validation.py` — 200-trial Monte Carlo, auto-cal achieves 0.3–0.5° mean error
- `test_autocal_e2e.py` — Full GPS→ephemeris→calibrate→direction pipeline
- `simulate.py` — Desktop simulator with noise modeling and accuracy sweep

## Web Dashboard

```bash
# Start dashboard (connects to public MQTT broker by default)
pip install -r web_dashboard/requirements.txt
python web_dashboard/app.py

# Custom broker
python web_dashboard/app.py --broker 192.168.1.1 --port 5000

# Docker
docker compose up -d
# Open http://localhost:5000
```

Features:
- **Overview** — multi-ball cards with compass, SOC bar, confidence bar, faults
- **Ball Detail** — full direction vector, panel angles, raw bitmask, timestamp
- **History** — SQLite-backed query by ball ID, configurable limit
- **OTA Update** — send OTA command via web UI to any ball
- **SSE streaming** — real-time updates without polling
- **REST API** — `/api/balls`, `/api/ball/<id>`, `/api/history/<id>`, `/api/ota`, `/api/stats`

## Key Design Decisions

**Why a ball?** A sphere provides hemispherical coverage with symmetric cosine response. 80 ALS-PT19 phototransistors on a Fibonacci lattice give near-uniform ~12° spacing.

**Why 80 sensors?** Each sensor contributes a weighted vote. The 3-sensor minimum for valid results is easily met even under heavy cloud cover.

**Why 4G instead of WiFi/LoRa?** 4G provides global coverage at km distances. The SIM7600G module handles TCP/MQTT natively via AT commands, simplifying firmware. IoT SIM ~¥60/year.

**Why auto-calibration with the sun?** Hand-wiring 80 sensors inevtably scrambles the channel-to-position mapping. Traditional calibration requires 80 manual flashlight alignments. Using the sun as a known light source via solar ephemeris, one outdoor scan is sufficient.

**Why MIP reflective display?** Sharp Memory LCD draws zero power to maintain an image (each pixel is a tiny capacitor). The screen stays readable during ESP32 deep sleep, in direct sunlight, with no backlight. Only needs 3 SPI wires (MOSI, SCLK, CS) — zero additional GPIO conflicts.

**Why onboard diagnostics?** Detecting sensor faults, overcast conditions, and low-confidence measurements at the edge allows the central controller to make informed decisions (ignore unreliable data, schedule maintenance, etc.) without polling.

## License

MIT — use, modify, sell freely with attribution.
