# Dust & Sand Protection Design — Solar Ball v1.1

## Problem

The Solar Ball is deployed outdoors 24/7. In arid or desert environments,
fine sand and dust accumulate on the ball surface, scattering incoming light
and reducing sensor accuracy. Over time, this can cause:

- Uniform reduction in all sensor readings (treated as overcast)
- Direction bias toward the cleaner side of the ball
- Accelerated wear on the PETG shell surface

## Solution: Replaceable Filter Mesh + Self-Cleaning Geometry

### 1. Hydrophobic Mesh Cover

A thin, black polyester mesh (200-micron openings) is stretched over the
ball surface. The mesh:

- Blocks sand particles >200μm (desert sand is typically 100-500μm)
- Dust <100μm passes through but has minimal scattering effect
- Hydrophobic coating causes water droplets to bead and roll off
- Black color absorbs stray light, reducing internal reflections
- Mesh transparency ~85% (acceptable insertion loss)

```
        Sunlight
           ↓
  ┌─────────────────────┐  ← Mesh outer layer
  │  ░░░░░░░░░░░░░░░░ │     (200μm openings, hydrophobic)
  │  ░░░░░░░░░░░░░░░░ │
  └────────┬────────────┘
           ↓
  ╔════════╧════════════╗  ← Ball shell (PETG, 2mm)
  ║     Sensor hole     ║
  ║     (ALS-PT19)      ║
  ║                     ║
  ╚═════════════════════╝
```

### 2. Self-Cleaning Geometry

The ball's spherical shape naturally sheds debris:

- **Smooth surface finish**: PETG shell polished to Ra <0.4μm
- **Steep curvature**: Particles slide off at angles >30° from top
- **Morning dew**: Condensation washes the surface daily
- **Rain**: Natural cleaning event

For arid regions (no rain), add a **vibration cleaning cycle**:
- ESP32 drives a small piezo buzzer (GPIO 4, shared with display CS)
- Brief 2-second buzz every 6 hours during daytime
- Dislodges accumulated dust via micro-vibrations
- Power cost: ~5mA for 2 seconds = negligible

### 3. Replaceable Outer Sock

For extreme environments, an outer "sock" made of PTFE (Teflon) fabric:

```
                    ┌─────────────────────┐
                    │  PTFE fabric sock    │
                    │  (replaceable)       │
                    │                      │
                    │  ┌───────────────┐   │
                    │  │   Ball shell  │   │
                    │  │   (100mm)     │   │
                    │  └───────────────┘   │
                    │                      │
                    └─────────────────────┘
                          ↓ elastic band
```

- PTFE is extremely hydrophobic (water contact angle >120°)
- The sock can be pulled off and replaced in <30 seconds
- No tools required
- Cost: ~¥5 per sock, replace every 6-12 months
- Available in white (high reflectivity) or black (anti-glare)

## BOM Addition

| # | Component | Spec | Qty | Cost |
|---|-----------|------|:---:|------|
| 25 | Hydrophobic mesh | Polyester, 200μm, 200×200mm | 1 | ¥3 |
| 26 | PTFE outer sock | White, 120mm diameter, elastic band | 2 | ¥10 |
| 27 | Piezo buzzer | 5V, 12mm, for vibration cleaning | 1 | ¥2 |
| 28 | Resistor 100Ω | 0805, piezo current limit | 1 | ¥0.1 |

**Additional cost: ~¥15.1**

## Vibration Cleaning Implementation

```c
// In power.c or a new maintenance module
void dust_clean_cycle(void) {
    // GPIO 4 drives piezo via MOSFET or direct if current <40mA
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    for (int i = 0; i < 200; i++) {
        gpio_set_level(GPIO_NUM_4, 1);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(GPIO_NUM_4, 0);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    // Total: 2 seconds, ~5mA average
}
```

Trigger every 6 hours during daytime (SOC > 20% to avoid draining battery).

## Sensor Health Monitoring

The new `sensor_diag.c` module already detects:

- `FAULT_SENSOR_OPEN` (0x0001): sensor disconnected or dirty to opacity
- `FAULT_SENSOR_SHORT` (0x0002): sensor shorted to ground
- `FAULT_SENSOR_SATURATED` (0x0004): sensor in saturation

If a sensor becomes progressively dirtier, its raw ADC rises toward
OPEN_THRESHOLD. The diag module reports this as a fault. The central
computer can correlate fault counts over time to schedule maintenance.

A sensor that reads consistently >90% of max (OPEN_THRESHOLD) for more
than 24 consecutive hours is flagged as "clean me" — the outer sock
likely needs replacement.

## Cleaning Schedule

| Environment | Sock replacement | Mesh cleaning | Vibration |
|-------------|:----------------:|:-------------:|:---------:|
| Urban | 12 months | Rain only | Disabled |
| Rural/agricultural | 6 months | Monthly | Monthly |
| Desert/arid | 3 months | Weekly | Daily |
| Coastal (salt) | 6 months | Monthly | Weekly |
