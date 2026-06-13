# Solar Ball — Directional Light Sensor for Solar Arrays

[![Tests](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/test.yml/badge.svg)](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/test.yml)
[![Firmware Build](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/firmware.yml/badge.svg)](https://github.com/yuchenmax88-wq/solar-ball/actions/workflows/firmware.yml)

A self-contained, self-powered directional light sensor that tells solar arrays where the sun is. Uses 80 phototransistors embedded on a 100mm sphere to measure the brightest sky direction, computes a unit vector, and transmits it via 4G MQTT to a central computer that adjusts solar panel orientation.

## How It Works

```
                ┌─────────────────┐
 Sky light ────→│  80 ALS-PT19    │
     ↓          │  on sphere      │
                └────────┬────────┘
                         │
                ┌────────v────────┐
                │  5× CD74HC4067  │  analog multiplexers
                │  (80 ch → 5 ch) │
                └────────┬────────┘
                         │
                ┌────────v────────┐
                │  2× ADS1115     │  16-bit I2C ADC
                └────────┬────────┘
                         │ I2C
                ┌────────v────────┐
                │  ESP32 MCU      │  weighted centroid
                │  direction algo │  algorithm
                └────────┬────────┘
                         │ UART
                ┌────────v────────┐
                │  SIM7600G 4G    │  MQTT over AT
                └────────┬────────┘
                         │ 4G
                ┌────────v────────┐
                │  MQTT Broker    │  /solar/ball/{id}/direction
                └────────┬────────┘
                         │
                ┌────────v────────┐
                │  Array Tracker  │  adjusts panel orientation
                └─────────────────┘
```

## Specifications

| Parameter | Value |
|-----------|-------|
| Sensors | 80 × ALS-PT19 phototransistors |
| Sphere | 100mm diameter, Fibonacci lattice layout |
| Direction accuracy | 0.3–0.5° (with calibration), 3–5° (without) |
| Update interval | 5 seconds |
| Communication | SIM7600G 4G Cat-4, MQTT QoS 1 |
| Power | 5W flexible solar panel + 18650 Li-ion (2000 mAh) |
| Battery life | ~3 days without sun, indefinite with partial sun |
| BOM cost | ~¥372 (~$50 USD) |
| Mounting | Fixed pole, independent of array mechanics |
| MCU | ESP32-WROOM-32 (ESP-IDF / PlatformIO) |
| Weight | ~300g (estimated, without pole) |

## Energy Efficiency Gain

Dual-axis tracking vs fixed-tilt panels:

| Region | Latitude | Annual Gain |
|--------|:--------:|:-----------:|
| Beijing | 40°N | +35–40% |
| Shanghai | 31°N | +30–35% |
| Shenzhen | 22°N | +25–30% |

Winter gains are higher (50–100%) since the sun stays low; summer gains are 15–25%. With the Solar Ball's 0.3–0.5° tracking accuracy, misalignment losses are under 0.005%.

## Project Structure

```
solar-ball/
├── README.md
├── hardware/
│   ├── BOM.md                       Bill of materials
│   ├── schematic.md                 Circuit schematic + GPIO map
│   └── ball_design.md               3D printed shell design
├── firmware/
│   ├── platformio.ini               PlatformIO build config
│   ├── CMakeLists.txt               ESP-IDF CMake build
│   ├── include/
│   │   ├── config.h                 Pin mappings, broker, APN
│   │   ├── sensor_positions.h       80 Fibonacci unit vectors
│   │   ├── sensor_calib.h           Calibration data structures
│   │   └── mqtt_protocol.h          MQTT wire format
│   ├── src/
│   │   ├── main.c                   Boot → scan → compute → publish → sleep
│   │   ├── sensor_scan.c/.h         80-ch MUX + ADS1115 ADC driver
│   │   ├── direction.c/.h           Weighted centroid algorithm
│   │   ├── mqtt_4g.c/.h             SIM7600G AT command MQTT driver
│   │   ├── power.c/.h               Battery ADC + deep sleep
│   │   ├── calibrate.c/.h           NVS storage + auto-calibration
│   │   └── sun_calc.c/.h            NOAA solar ephemeris
│   ├── main/
│   │   └── CMakeLists.txt            ESP-IDF component registration
│   └── scripts/
│       └── generate_sensor_coords.py  Fibonacci sphere coordinate generator
├── tools/
│   ├── auto_calibrate.py            Calibration tool (manual + auto modes)
│   └── requirements.txt
├── tests/
│   ├── test_direction.py            Direction algorithm tests
│   ├── test_mqtt_protocol.py        Serialization tests
│   ├── test_power.py                Battery SOC tests
│   ├── test_calibration.py          Mapping inversion tests
│   ├── test_autocal_validation.py   Auto-cal accuracy validation
│   ├── test_autocal_e2e.py          End-to-end pipeline tests
│   ├── simulate.py                  Desktop simulator
│   └── run_all.py                   Test runner
└── docs/
    ├── assembly_guide.md            Build instructions
    └── calibration_procedure.md     Calibration guide
```

## Quick Start

### Generate sensor coordinates

```bash
cd firmware/scripts
python generate_sensor_coords.py
```

### Build firmware

```bash
cd firmware
platformio run
```

### Flash to ESP32

```bash
platformio run --target upload
```

### Run tests

```bash
cd tests
python run_all.py
```

## Calibration

Two methods are supported:

### Method 1: Sun Auto-Calibration (Recommended)

```bash
python tools/auto_calibrate.py COM3 --auto
```

Place the ball outdoors under clear sky. The system will:
1. Get GPS location from the 4G module (cell tower positioning)
2. Sync time via NTP
3. Compute the sun's exact direction from solar ephemeris
4. Scan all 80 sensors once
5. Automatically map each physical channel to its position on the sphere
6. Save the calibration to NVS flash

No manual steps required. One scan is sufficient.

### Method 2: Manual Flashlight Calibration

```bash
python tools/auto_calibrate.py COM3
```

Hold the BOOT button during power-on, then follow the prompts to shine a flashlight at each of the 80 numbered holes (one at a time).

## MQTT Protocol

Published every 5 seconds to `/solar/ball/{id}/direction`:

```json
{
  "id":   "ball-001",
  "ts":   1718000000,
  "dx":   0.3215,
  "dy":   -0.1478,
  "dz":   0.9352,
  "soc":  87,
  "rssi": -89
}
```

| Field | Type | Description |
|-------|------|-------------|
| `id`   | string | Ball identifier |
| `ts`   | uint32 | Unix timestamp (seconds) |
| `dx,dy,dz` | float | Unit vector toward brightest sky direction |
| `soc`  | uint8  | Battery state of charge (0–100%) |
| `rssi` | int8   | 4G signal strength (dBm) |

The direction vector always has unit length. The central computer uses it to compute the solar
array's optimal orientation independently per ball.

## Key Design Decisions

**Why a ball shape?** A sphere naturally provides hemispherical coverage with symmetric cosine response. The 80 ALS-PT19 phototransistors are placed on a Fibonacci sphere lattice, giving near-uniform angular distribution (~12° spacing). The bottom 30° has no sensors (the sun is never below ground).

**Why 80 sensors?** Each sensor contributes a weighted vote to the direction vector. With 80 sensors, the 3-sensor minimum for valid results is easily met even under heavy cloud cover. More sensors would increase cost and complexity linearly but only improve accuracy by sqrt(N).

**Why 4G instead of WiFi/LoRa?** 4G provides global coverage, works at km distances, and the SIM7600G module handles TCP/MQTT natively via AT commands — simplifying firmware. The IoT SIM costs ~¥60/year.

**Why auto-calibration with the sun?** Hand-wiring 80 sensors to 80 physical PCB channels inevitably results in scrambled mapping. Traditional calibration requires 80 manual flashlight alignments. By using the sun as a known light source (via solar ephemeris), one outdoor scan is sufficient with direction accuracy of 0.3–0.5°.

## License

MIT
