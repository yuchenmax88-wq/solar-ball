# Solar Ball — Directional Light Sensor for Solar Arrays

[![Tests](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/test.yml/badge.svg)](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/test.yml)
[![Firmware Build](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/firmware.yml/badge.svg)](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/firmware.yml)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.1.0-green)](firmware/main/config.h)

A self-contained, self-powered directional light sensor that tells solar arrays where the sun is.  
80 phototransistors on a 100mm sphere → weighted centroid → direction vector → 4G MQTT → array tracking.  
**BOM ~¥372 / ~$50. Direction accuracy 0.3–0.5° (calibrated).**

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
                                       (OLED)       (JSON via 4G)
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
| Display | SSD1306 128×64 OLED (I2C, auto-sleep) |
| Power | 5W solar panel + 18650 2000mAh |
| BOM cost | **~¥372 (~$50)** |
| Firmware | C, 9 modules, ~2000 lines |
| Tests | 7 suites, **389 assertions**, all passing |

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
| **Display** | SSD1306 OLED status screen (I2C, µA sleep) | ✅ |
| | Shows direction, azimuth, elevation, SOC, RSSI, faults, confidence bar | ✅ |
| **Safety** | Overcast/night → auto-point zenith, low confidence | ✅ |
| | Low battery fault flag | ✅ |
| | Modem/MQTT failure flags in payload | ✅ |
| **Hardware** | Dust/sand protection design (replaceable PTFE sock) | ✅ |
| | Piezo vibration cleaning (optional) | ✅ |
| **Tooling** | PC calibration tool (serial UART, manual + auto modes) | ✅ |
| | MQTT receiver (subscribe → vector → panel angles) | ✅ |
| | Desktop simulator (full pipeline, noise modeling) | ✅ |
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
│       ├── CMakeLists.txt          Component registration (9 source files)
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
│       ├── display.h/.c            SSD1306 OLED I2C driver (5×7 font)
│       ├── remote_diag.h/.c        Scan/publish counters
│       └── scripts/
│           └── generate_sensor_coords.py  Fibonacci sphere → C header
├── tools/
│   ├── auto_calibrate.py           Calibration tool (--auto for sun, default for flashlight)
│   ├── mqtt_receiver.py            MQTT subscriber: (dx,dy,dz) → azimuth/elevation/panel angles
│   └── requirements.txt            pyserial, colorama, paho-mqtt
├── tests/
│   ├── run_all.py                  Test runner (7 suites)
│   ├── test_direction.py           38 assertions — algorithm correctness
│   ├── test_mqtt_protocol.py       25 assertions — JSON serialization
│   ├── test_power.py               20 assertions — battery SOC calculation
│   ├── test_calibration.py         246 assertions — channel mapping inversion
│   ├── test_weather.py             25 assertions — overcast/night/fault classification
│   ├── test_saturation.py          15 assertions — saturation detection
│   ├── test_remote_diag.py         20 assertions — diagnostic protocol
│   ├── test_autocal_validation.py  200-trial Monte Carlo — auto-cal accuracy
│   ├── test_autocal_e2e.py         GPS→ephemeris→calibrate→direction pipeline
│   └── simulate.py                 Desktop simulator (noise, accuracy sweep)
├── hardware/
│   ├── BOM.md                      ¥372 bill of materials
│   ├── schematic.md                Full circuit + GPIO pin map
│   ├── ball_design.md              3D shell + sensor placement design
│   └── dust_protection.md          Replaceable PTFE sock + vibration cleaning
├── docs/
│   ├── assembly_guide.md           Step-by-step build instructions
│   └── calibration_procedure.md    Auto + manual calibration guide
├── .github/workflows/
│   ├── test.yml                    Python tests on push
│   └── firmware.yml                PlatformIO build on push
├── LICENSE                         MIT
└── README.md
```

## Quick Start

```bash
# Generate sensor coordinates
cd firmware/scripts && python generate_sensor_coords.py

# Build firmware
cd firmware && platformio run

# Flash to ESP32
platformio run --target upload

# Run test suite (389 assertions, zero dependencies beyond Python 3)
cd tests && python run_all.py

# Desktop simulation (no hardware needed)
python tests/simulate.py                # random sun positions
python tests/simulate.py --scan         # full sky sweep
python tests/simulate.py --noiseless    # ideal sensor accuracy test
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
  test_weather.py           — 25 passed    overcast/night/fault classification
  test_saturation.py        — 15 passed    saturation threshold detection
  test_remote_diag.py       — 20 passed    diagnostic protocol
============================================================
All tests passed!  (389 assertions, zero failures)
```

Additional validation scripts:
- `test_autocal_validation.py` — 200-trial Monte Carlo, auto-cal achieves 0.3–0.5° mean error
- `test_autocal_e2e.py` — Full GPS→ephemeris→calibrate→direction pipeline
- `simulate.py` — Desktop simulator with noise modeling and accuracy sweep

## Key Design Decisions

**Why a ball?** A sphere provides hemispherical coverage with symmetric cosine response. 80 ALS-PT19 phototransistors on a Fibonacci lattice give near-uniform ~12° spacing.

**Why 80 sensors?** Each sensor contributes a weighted vote. The 3-sensor minimum for valid results is easily met even under heavy cloud cover.

**Why 4G instead of WiFi/LoRa?** 4G provides global coverage at km distances. The SIM7600G module handles TCP/MQTT natively via AT commands, simplifying firmware. IoT SIM ~¥60/year.

**Why auto-calibration with the sun?** Hand-wiring 80 sensors inevtably scrambles the channel-to-position mapping. Traditional calibration requires 80 manual flashlight alignments. Using the sun as a known light source via solar ephemeris, one outdoor scan is sufficient.

**Why an OLED display?** Shows real-time status at negligible power cost (µA sleep, ~5mA during brief refresh). Shares the existing I2C bus with the ADCs — no extra pins needed.

**Why onboard diagnostics?** Detecting sensor faults, overcast conditions, and low-confidence measurements at the edge allows the central controller to make informed decisions (ignore unreliable data, schedule maintenance, etc.) without polling.

## License

MIT — use, modify, sell freely with attribution.
