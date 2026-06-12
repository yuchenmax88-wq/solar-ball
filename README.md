# Solar Ball — Maximum-Power-Point Direction Sensor

A self-contained, self-powered directional light sensor for photovoltaic arrays.
Measures the brightest direction in the sky using 80 phototransistors on a
spherical surface, computes a direction vector, and transmits it via 4G MQTT.

## How It Works

```
                    ┌─────────────────┐
  Sky light ───────►│  80 ALS-PT19    │
     ↓              │  on ball surface │
                    └────────┬────────┘
                             │
                    ┌────────v────────┐
                    │  5× CD74HC4067  │
                    │  MUX (80→5)     │
                    └────────┬────────┘
                             │
                    ┌────────v────────┐
                    │  2× ADS1115     │
                    │  16-bit ADC     │
                    └────────┬────────┘
                             │ I2C
                    ┌────────v────────┐
                    │  ESP32 MCU      │
                    │  Weighted       │
                    │  centroid       │
                    │  algorithm      │
                    └────────┬────────┘
                             │ UART
                    ┌────────v────────┐
                    │  SIM7600G 4G    │
                    │  MQTT publish   │
                    └────────┬────────┘
                             │ 4G / IoT SIM
                    ┌────────v────────┐
                    │  Central        │
                    │  Computer        │
                    │  (array tracking)│
                    └─────────────────┘
```

## Key Specifications

| Parameter | Value |
|-----------|-------|
| Sensors | 80 × ALS-PT19 phototransistors |
| Effective resolution | ~5° (interpolated from ~12° sensor spacing) |
| Update interval | 5 seconds |
| Communication | 4G Cat-4 (SIM7600G) + MQTT |
| Self-powered | 5W solar panel + 18650 battery |
| Battery life | ~3 days (no sun), indefinite (sunny) |
| Range | Anywhere with 4G coverage (>95% of China plains) |
| Ball diameter | 100mm |
| Mounting | Fixed pole, independent of solar array |

## Project Structure

```
solar-ball/
├── README.md                     ← This file
├── hardware/
│   ├── BOM.md                    ← Bill of materials (~¥372)
│   ├── schematic.md              ← Circuit schematic + GPIO map
│   └── ball_design.md            ← 3D printed shell design guide
├── firmware/
│   ├── platformio.ini            ← ESP32 build config (PlatformIO)
│   ├── include/
│   │   ├── config.h              ← Pin mappings, MQTT broker, etc.
│   │   ├── sensor_positions.h    ← 80 sensor unit vectors (auto-generated)
│   │   ├── sensor_calib.h        ← Calibration data structures
│   │   └── mqtt_protocol.h       ← MQTT wire format definition
│   ├── src/
│   │   ├── main.c                ← Startup → scan → publish → sleep
│   │   ├── sensor_scan.c/.h      ← MUX + ADC driver, 80-channel scan
│   │   ├── direction.c/.h        ← Weighted centroid algorithm
│   │   ├── mqtt_4g.c/.h          ← SIM7600G driver + MQTT AT commands
│   │   ├── power.c/.h            ← Battery monitoring + deep sleep
│   │   └── calibrate.c/.h        ← NVS storage + auto-calibration mode
│   └── scripts/
│       └── generate_sensor_coords.py  ← Generates Fibonacci sphere positions
├── tools/
│   ├── auto_calibrate.py         ← PC tool: sequential illumination mapping
│   └── requirements.txt
└── docs/
    ├── assembly_guide.md          ← Step-by-step build instructions
    └── calibration_procedure.md   ← How to run auto-calibration
```

## Quick Start

### 1. Generate sensor positions

```
cd firmware/scripts
python generate_sensor_coords.py
```

### 2. Build firmware

```
cd firmware
platformio run
```

### 3. Flash to ESP32

```
platformio run --target upload
```

### 4. Run auto-calibration (after assembly)

```
cd tools
pip install -r requirements.txt
python auto_calibrate.py COM3
```

### 5. Deploy

Hold BOOT button while powering on to enter calibration mode.
Release BOOT button for normal operation.

## MQTT Data Format

Published every 5 seconds to topic `/solar/ball/{id}/direction`:

```json
{
  "id":  "ball-001",
  "ts":  1718000000,
  "dx":  0.3215,
  "dy":  -0.1478,
  "dz":  0.9352,
  "soc": 87,
  "rssi": -89
}
```

- `dx, dy, dz`: Unit vector pointing toward brightest sky direction
- `soc`: Battery charge (0-100%)
- `rssi`: 4G signal strength (dBm)

The central computer receives this and calculates array orientation
(zero mechanical delay since ball is independent of the array).
