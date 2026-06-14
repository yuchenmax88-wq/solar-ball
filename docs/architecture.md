# Solar Ball Architecture

## System Overview

```
 ┌──────────────────────────────────────────────────────────────────────────┐
 │                           SOLAR BALL DEVICE                              │
 │                                                                          │
 │  ┌─────────┐   ┌──────────┐   ┌─────────┐   ┌──────────┐   ┌─────────┐ │
 │  │ 80 ALS- │──▶│ 5×16-ch  │──▶│2×ADS1115│──▶│  ESP32   │──▶│  Sharp  │ │
 │  │  PT19   │   │   MUX    │   │   ADC   │   │ WROOM-32 │   │MIP LCD  │ │
 │  └─────────┘   └──────────┘   └─────────┘   └────┬─────┘   └─────────┘ │
 │                                                   │                     │
 │    ┌──────────┐   ┌──────────┐   ┌────────────┐  │  ┌──────────┐        │
 │    │ 5W Solar │──▶│  TP4056  │──▶│18650 2000mAh│──│─▶│ DC-DC 3.3│        │
 │    └──────────┘   └──────────┘   └────────────┘  │  └──────────┘        │
 │                                                   │                     │
 │                                          ┌────────┴────────┐            │
 │                                          │   SIM7600G 4G   │            │
 │                                          │  UART AT cmds   │            │
 │                                          └────────┬────────┘            │
 └───────────────────────────────────────────────────┼──────────────────────┘
                                                     │ 4G LTE
                                                     ▼
 ┌──────────────────────────────────────────────────────────────────────────┐
 │                              CLOUD / EDGE                                 │
 │                                                                          │
 │  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                │
 │  │ MQTT Broker  │◀──▶│   Web Dash   │◀──▶│MQTT Receiver │                │
 │  │(eclipse-mosq)│    │  (Flask+SSE) │    │  (Python)    │                │
 │  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘                │
 │         │                   │                    │                        │
 │         ▼                   ▼                    ▼                        │
 │  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                │
 │  │   SQLite DB  │    │  Chart.js UI │    │Array Motor   │                │
 │  │   (history)  │    │  (real-time) │    │ Controller   │                │
 │  └──────────────┘    └──────────────┘    └──────────────┘                │
 └──────────────────────────────────────────────────────────────────────────┘
```

## Firmware Architecture

### Boot Sequence

```
Power-on Reset
     │
     ▼
 NVS init → OTA validation → Power init → Calibration load
     │
     ▼
 [BOOT button held?]──YES──▶ Calibration mode (UART)
     │
     NO
     │
     ▼
 Sensor scan (80-ch MUX + ADS1115 + normalization)
     │
     ├──▶ Raw ADC values → sensor_diag (faults, weather, confidence)
     │                       │
     ▼                       ▼
 direction_compute()    error_mask, confidence, flags
 (weighted centroid)
     │
     ▼
 Display update (Sharp MIP LCD — persists during deep sleep)
     │
     ▼
 Modem init → NTP sync → MQTT subscribe (cmd topic)
     │
     ├──▶ Poll OTA commands → download + flash (if triggered)
     │
     ▼
 MQTT publish direction → power off modem → deep sleep
     │
     ▼
 [Wake after DEEP_SLEEP_WAKEUP_S] → repeat
```

### Module Dependency Graph

```
main.c
 ├── config.h           (all hardware pin/parameter defines)
 ├── sensor_positions.h  (80 Fibonacci unit vectors, auto-generated)
 ├── sensor_scan.h/.c    → I2C driver (driver/i2c)
 ├── sensor_diag.h/.c    → depends on sensor_scan, calibrate
 ├── direction.h/.c      → depends on sensor_positions
 ├── mqtt_4g.h/.c        → depends on mqtt_protocol, UART driver
 │   ├── MQTT publish
 │   ├── MQTT subscribe
 │   ├── HTTP download (chunked)
 │   └── NTP sync
 ├── power.h/.c          → depends on ADC driver
 ├── calibrate.h/.c      → depends on NVS, sun_calc
 ├── sun_calc.h/.c       → NOAA solar ephemeris (static computation)
 ├── display.h/.c        → Sharp LCD SPI (bit-bang GPIO)
 ├── remote_diag.h/.c    → scan/publish counters
 ├── ota.h/.c            → depends on esp_ota, mqtt_4g
 └── remote_config.h/.c  → depends on NVS, MQTT commands
```

### Data Flow Through Modules

```
                     sensor_scan_all_with_raw()
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
      float sensor_vals[80]      uint16_t raw_avgs[80]
      (normalized 0.0-1.0)        (raw ADC readings)
              │                           │
              ▼                           ▼
      direction_compute()       sensor_diag_analyze()
              │                           │
              ▼                           ▼
      vec3_t direction         uint16_t error_mask
      (dx, dy, dz)             uint8_t confidence
              │                uint8_t flags
              └───────────┬───────────────┘
                          ▼
                 direction_packet_t
                 (serialized to JSON)
                          │
                          ▼
                 mqtt_publish_direction()
                          │
                          ▼
                  4G → MQTT Broker → Subscribers
```

## Communication Protocol

### Direction Publish (ball → broker)
- **Topic**: `/solar/ball/{ball_id}/direction`
- **QoS**: 1 (at least once)
- **Interval**: SENSOR_SCAN_INTERVAL_S (default 5s)
- **Payload**: JSON with 10 fields (see README)

### Command Receive (broker → ball)
- **Topic**: `/solar/ball/{ball_id}/cmd`
- **QoS**: 1
- **Supported commands**: `ota`, `config` (future)
- **Payload**: JSON with `cmd` field + parameters

## Calibration System

### Three Calibration Methods

| Method | Input | Time | Accuracy | Auto? |
|--------|-------|------|----------|-------|
| Sun Auto-Cal | 1 outdoor scan + GPS location | ~10s | 0.3–0.5° | Yes |
| Flashlight Manual | 80-step flashlight alignment | ~5 min | 0.2–0.3° | Guided |
| Baseline Normalization | Uniform diffuse light | ~10s | — | Semi |

### Channel Mapping Algorithm (Sun Auto-Cal)

```
1. Get sun direction from NOAA ephemeris (lat, lon, time)
2. Scan all 80 physical channels → raw ADC values
3. Sort channels by brightness (ascending raw = brighter)
4. For each position on Fibonacci sphere:
     predicted = max(0, dot(position_normal, sun_direction))
5. Sort positions by predicted brightness
6. Greedy map: nth-brightest physical → nth-brightest position
7. Store mapping in NVS
```

## Diagnostic System

### Fault Detection Pipeline

```
raw ADC per channel
       │
       ▼
sensor_diag_check_channel(phys)
       │
       ├── raw > 90% full-range → OPEN (stuck dark)
       ├── raw < 2% full-range → SHORT (stuck bright)
       └── raw < 5% full-range in daylight → SATURATED
       │
       ▼
Aggregate faults → error_mask bitmask (10 bits)
       │
       ▼
Weather classification:
  ├── variance/mean < 0.15 → OVERCAST
  └── all sensors dark + uniform → NIGHT
       │
       ▼
Confidence scoring (0-255):
  ├── Start at 255
  ├── Subtract per-fault penalty (−5 each)
  ├── Overcast: ×0.4
  └── Night: ×0.1
```

## Power Management

```
Solar Panel (5W) → TP4056 → 18650 (2000mAh)
                           │
                    ┌──────┴──────┐
                    ▼              ▼
            ADC1 (GPIO36)    DC-DC 3.3V → ESP32 + peripherals
                    │
                    ▼
          battery_mv → SOC (%)
          (linear interpolation)
                    │
                    ▼
          Deep sleep timer (SENSOR_SCAN_INTERVAL_S)
```

- **Runtime**: ~5s active per cycle
- **Standby current**: <100µA (deep sleep)
- **Battery life**: ~3 days without solar input
- **SOC interpolation**: 3200mV = 0%, 4200mV = 100%
- **Low battery threshold**: SOC < 10%

## Test Architecture

### Layer Testing Strategy

```
 ┌─────────────────────────────────────────┐
 │          Simulation (simulate.py)        │  Pure Python, no hardware
 │     Sensor model → direction → error    │
 ├─────────────────────────────────────────┤
 │       Integration (test_integration.py)  │  E2E pipeline, MQTT, OTA
 │   Real protocol + simulated sensors     │
 ├─────────────────────────────────────────┤
 │         Unit Tests (7 test_*.py files)   │  Algorithm correctness
 │     direction, mqtt, power, calib...    │  (418 assertions)
 ├─────────────────────────────────────────┤
 │    Monte Carlo (test_autocal_validation) │  200-trial accuracy
 │     Calibration robustness testing      │
 └─────────────────────────────────────────┘
```

## Web Dashboard Architecture

```
Browser (SSE + Chart.js + vanilla JS)
       │
       ▼ EventSource
Flask (app.py)
  ├── SSE stream (push real-time data)
  ├── REST API (/api/balls, /api/history, /api/ota, /api/stats)
  ├── MQTT client thread (subscribe to /solar/ball/+/direction)
  └── SQLite writer thread (log every reading)
       │
       ▼
MQTT Broker (Mosquitto)
       │
       ▼
Solar Ball devices (publishers)
```

## Security Considerations

- MQTT over TCP (not TLS) — acceptable for telemetry data on private APN
- No authentication on the ball — the SIM7600G APN provides network isolation
- OTA firmware URLs are user-configured; validate in the dashboard before sending
- NVS persists calibration and OTA validation flags across reboots
- OTA rollback: if boot fails, the previous partition is retained

## Performance Characteristics

| Metric | Value |
|--------|-------|
| Direction accuracy | 0.3–0.5° (calibrated) |
| Scan duration | ~1.5s (80 sensors × 8× oversample) |
| MQTT publish latency | ~2s (modem init + connect + publish) |
| Cycle total | ~5s (scan + compute + publish) |
| Flash usage | ~800KB (of 4MB) |
| RAM usage | ~120KB (of 520KB DRAM) |
| OTA download speed | ~50–200 KB/s (4G dependent) |
