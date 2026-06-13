# Solar Ball — Complete Operation Manual (English)

> **Audience**: Absolute beginners. If you can use a screwdriver and plug in a USB cable, you can do this.  
> **Estimated time**: Hardware assembly 4–6 hours, software setup 15 minutes, calibration 10 minutes.

---

## Table of Contents

1. [What You Need](#1-what-you-need)
2. [Hardware Pin Connections](#2-hardware-pin-connections)
3. [Wiring Quick Reference](#3-wiring-quick-reference)
4. [Assembly Overview](#4-assembly-overview)
5. [Install PC Software](#5-install-pc-software)
6. [Flash Firmware to ESP32](#6-flash-firmware-to-esp32)
7. [Outdoor Auto-Calibration](#7-outdoor-auto-calibration)
8. [Start the Receiver](#8-start-the-receiver)
9. [Connect Solar Array Motors](#9-connect-solar-array-motors)
10. [Daily Operation & Maintenance](#10-daily-operation--maintenance)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. What You Need

### Hardware (total ~$55, see `hardware/BOM.md` for full list)

| Category | Key Component | Qty |
|----------|-------------|:---:|
| MCU | ESP32-WROOM-32 dev board | 1 |
| Sensor | ALS-PT19 phototransistor | 80 |
| Analog MUX | CD74HC4067 (16-channel) | 5 |
| ADC | ADS1115 module (I2C) | 2 |
| 4G Module | SIM7600G-H | 1 |
| 4G Antenna | SMA male, 3-5 dBi, magnetic base | 1 |
| IoT SIM | 10 MB/month data plan | 1 |
| Battery | 18650 Li-ion 3.7V 2000 mAh | 1 |
| Charger | TP4056 module | 1 |
| Boost | MT3608 module (adjust to 5V output) | 1 |
| Regulator | AMS1117-3.3 module | 1 |
| Solar Panel | 5V 5W flexible | 1 |
| Resistor | 10kΩ (one per sensor) | 80 |
| Resistor | 100kΩ (battery divider pair) | 2 |
| Resistor | 4.7kΩ (I2C pull-up pair) | 2 |
| OLED | SSD1306 128×64 I2C | 1 |
| 3D Print | Ball shell (PETG, 100mm) + internal frame | 1 |
| Misc | Jumper wires, pin headers, heat shrink, silicone sealant | lots |

### Tools

| Tool | Purpose |
|------|---------|
| Soldering iron (350°C / 660°F) | Solder sensors and headers |
| Multimeter | Continuity and voltage checks |
| Hot glue gun | Secure sensors to shell |
| Small Phillips screwdriver | Fastening |
| USB-TTL adapter | ESP32 serial (if dev board lacks USB) |
| Micro-USB cable | Power and flash ESP32 |

### PC Software

| Software | Purpose | Install |
|----------|---------|---------|
| Python 3.10+ | Run calibration tool & receiver | [python.org](https://python.org) |
| PlatformIO | Compile & flash firmware | `pip install platformio` |
| Serial driver | CP210x / CH340 | Auto-installed on Windows 10+ |

---

## 2. Hardware Pin Connections

### Full Pin Map

```
                    ESP32-WROOM-32 (38-pin dev board)
                   ┌────────────────────────────┐
       BOOT ───────┤ GPIO 0              GPIO 23├────── SIM7600G PWR_KEY
    MODEM_RST ─────┤ GPIO 2              GPIO 22├────── I2C1 SCL (ADS1115#1 + OLED)
     OLED_CS ──────┤ GPIO 4              GPIO 21├────── I2C1 SDA (ADS1115#1 + OLED)
     OLED_DC ──────┤ GPIO 5              GPIO 19├────── I2C2 SCL (ADS1115#2)
    MUX_EN2 ───────┤ GPIO 12             GPIO 18├────── I2C2 SDA (ADS1115#2)
    MUX_EN3 ───────┤ GPIO 13             GPIO 17├────── SIM7600G TX
    MUX_EN1 ───────┤ GPIO 14             GPIO 16├────── SIM7600G RX
    MUX_EN4 ───────┤ GPIO 15             GPIO 36├────── BAT_ADC (battery voltage)
     3.3V ─────────┤ 3V3                 GPIO 32├────── MUX_S0
       GND ────────┤ GND                 GPIO 33├────── MUX_S1
                   │                     GPIO 25├────── MUX_S2
                   │                     GPIO 26├────── MUX_S3
                   │                     GPIO 27├────── MUX_EN0
                   │                     GPIO 14├────── MUX_EN1
                   │                     EN (RST)├── Reset
                   │                     5V/VIN  ├── 5V power input
                   └────────────────────────────┘
```

### Pin Reference Tables

#### MUX Select Lines (shared across all 5 MUX chips)

| ESP32 GPIO | Connect To | Function |
|:----------:|-----------|----------|
| **32** | All MUX pin S0 | Channel select bit 0 |
| **33** | All MUX pin S1 | Channel select bit 1 |
| **25** | All MUX pin S2 | Channel select bit 2 |
| **26** | All MUX pin S3 | Channel select bit 3 |

#### MUX Enable Lines (one per MUX chip, active LOW)

| ESP32 GPIO | Connect To | Sensors Covered |
|:----------:|-----------|:--------------:|
| **27** | MUX #1 EN pin | Sensor 0–15 |
| **14** | MUX #2 EN pin | Sensor 16–31 |
| **12** | MUX #3 EN pin | Sensor 32–47 |
| **13** | MUX #4 EN pin | Sensor 48–63 |
| **15** | MUX #5 EN pin | Sensor 64–79 |

#### I2C Buses

| ESP32 GPIO | Connect To | I2C Address |
|:----------:|-----------|:-----------:|
| **21** (SDA) | ADS1115 #1 SDA + OLED SDA | 0x48, 0x3C |
| **22** (SCL) | ADS1115 #1 SCL + OLED SCL | — |
| **18** (SDA) | ADS1115 #2 SDA | 0x49 |
| **19** (SCL) | ADS1115 #2 SCL | — |

> **Important**: I2C requires 4.7kΩ pull-up resistors to 3.3V on both SDA and SCL lines.  
> The OLED and ADS1115#1 share I2C1 — different addresses, no conflict.

#### 4G Module (SIM7600G)

| ESP32 GPIO | SIM7600G Pin | Function |
|:----------:|:------------:|----------|
| **17** | TX (module RX) | ESP32 → module |
| **16** | RX (module TX) | module → ESP32 |
| **23** | PWR_KEY | Power-on pulse (pull LOW for 1.5s) |
| **2** | RESET | Hardware reset (not used by firmware) |

#### Other

| ESP32 GPIO | Connect To | Notes |
|:----------:|-----------|-------|
| **36** | Battery divider midpoint | Two 100kΩ in series, midpoint to GPIO36 |
| **0** | Push-button (BOOT) | LOW = pressed = enter calibration mode |
| **4** | OLED CS (reserved) | For e-ink expansion; current OLED uses I2C |
| **5** | OLED DC (reserved) | For e-ink expansion |

---

## 3. Wiring Quick Reference

### Single Sensor Wiring (repeat 80 times)

```
     3.3V ─── 10kΩ ─┬── MUX input pin (e.g. MUX#1 Y0)
                     │
                  ┌──┴──┐
                  │ ALS │  ALS-PT19 phototransistor
                  │PT19 │  (long leg = collector to resistor, short leg = emitter to GND)
                  └──┬──┘
                     │
                    GND
```

> **Critical**: Long leg (collector) → 10kΩ → 3.3V AND → MUX input. Short leg (emitter) → GND.  
> **Never reverse** — this will destroy the sensor.

### MUX → ADC Wiring

```
MUX #1 common pin → ADS1115#1 (0x48) AIN0
MUX #2 common pin → ADS1115#1 (0x48) AIN1
MUX #3 common pin → ADS1115#2 (0x49) AIN0
MUX #4 common pin → ADS1115#2 (0x49) AIN1
MUX #5 common pin → ADS1115#2 (0x49) AIN2
```

### Power Wiring (connect in order, test each step)

```
Solar 5V → TP4056 IN+  → TP4056 BAT+ → 18650 positive
                                  BAT- → 18650 negative
           TP4056 OUT+ → MT3608 IN+ → MT3608 OUT+ ┬→ AMS1117 IN → 3.3V (ESP32 + sensors)
                       MT3608 IN-  → MT3608 OUT- ┼→ SIM7600G 5V input
                                                   └→ All GND common

⚠ Check polarity with multimeter before connecting battery. Reverse = destroy everything.
⚠ Adjust MT3608 to exactly 5.0V before connecting load (turn potentiometer, measure with multimeter).
```

---

## 4. Assembly Overview

> Detailed steps in `docs/assembly_guide.md`

1. **Solder PCB**: Resistors first (10kΩ×80 + 100kΩ×2 + 4.7kΩ×2), then IC sockets and headers
2. **Solder sensors**: Each ALS-PT19 with 10cm wires + 10kΩ resistor, heat-shrink the joints
3. **Mount sensors in shell**: Insert from inside, hot-glue in place, lens flush with outer surface
4. **Assemble internal frame**: Secure ESP32, MUX boards, ADS1115s, SIM7600G, battery, power modules
5. **Connect 4G antenna**: SMA-to-IPEX cable to SIM7600G, magnetic base on shell exterior
6. **Insert SIM card**: Into SIM7600G's SIM slot
7. **Power-on test**: USB power first, check serial output at 115200 baud for boot messages
8. **Seal**: Apply silicone to all sensor holes and shell seams; cure 24 hours
9. **Mount solar panel**: On the flat top area, wires to TP4056 input
10. **Install on pole**: Bottom opening over support pole, ensure top points straight up (bubble level)

---

## 5. Install PC Software

Open a terminal:

```bash
# Install PlatformIO (compile & flash firmware)
pip install platformio

# Install project dependencies (calibration tool + receiver)
cd solar-ball/tools
pip install -r requirements.txt
```

---

## 6. Flash Firmware to ESP32

1. Connect ESP32 to PC via Micro-USB cable
2. Find your serial port (Windows: Device Manager → Ports, usually **COM3**; Linux: `/dev/ttyUSB0`; macOS: `/dev/cu.usbserial-*`)

Edit `firmware/platformio.ini`, set your serial port:

```ini
upload_port = COM3      # change to yours
monitor_port = COM3     # change to yours
```

Review `firmware/main/config.h` and adjust if needed:

```c
#define BALL_ID             "ball-001"         // your ball ID
#define MQTT_BROKER_HOST    "broker.emqx.io"   // public test broker, or your own
```

3. Compile & flash:

```bash
cd solar-ball/firmware
platformio run --target upload
```

`Success` means it's flashed.

4. Verify (optional):

```bash
platformio device monitor
```

You should see:

```
Solar Ball v1.1.0 booting...
Ball ID: ball-001
No calibration data found. Using default normalization.
```

---

## 7. Outdoor Auto-Calibration

> **Prerequisites**: Ball assembled, SIM card inserted, outdoors with clear sky.

### Method 1: One-Button Auto (Recommended)

Place ball outdoors, USB connected to PC:

```bash
cd solar-ball/tools
python auto_calibrate.py COM3 --auto
```

The ball will automatically:
1. Connect to 4G network
2. Get GPS location from cell tower
3. Sync time via NTP
4. Compute exact sun position (solar ephemeris)
5. Scan all 80 sensors once
6. Build the mapping
7. Save to permanent storage (NVS)

~60 seconds. Then unplug USB — the ball runs autonomously from now on.

### Method 2: Manual (Cloudy Day Backup)

1. Hold BOOT button while powering on → enters calibration mode
2. Open any serial terminal (115200 baud), send `CAL:SUN_AUTO`
3. Or run the 80-step flashlight process: send `CAL:START`, follow prompts

PC-guided manual mode:

```bash
python auto_calibrate.py COM3          # flashlight mode (without --auto)
```

### Optional: Baseline Calibration

In calibration mode, send:

```
CAL:BASELINE_START
```

Place the ball in uniform diffuse light (overcast sky or white cloth over the ball). 100 samples per sensor → stored as baseline normalization reference.

---

## 8. Start the Receiver

Once the ball is operating (publishing every 5 seconds), run:

```bash
cd solar-ball/tools
python mqtt_receiver.py
```

You'll see:

```
[ball-001] 14:32:05 Sun: az= 169.2° elev=+73.0°  Panel: az= 169.2° tilt=17.0°  SOC=87% RSSI=-89dBm conf=220
  Faults: none  Flags: [DIR_OK CALIB ]  vector:OK

[ball-001] 14:32:10 Sun: az= 169.5° elev=+72.8°  Panel: az= 169.5° tilt=17.2°  SOC=87% RSSI=-89dBm conf=220
  Faults: none  Flags: [DIR_OK CALIB ]  vector:OK
```

| Output Field | Meaning |
|-------------|---------|
| `az=169° elev=+73°` | Sun at azimuth 169° (south-by-east), elevation 73° |
| `Panel: az=169° tilt=17°` | **Direct values for motor controller**: rotate to 169°, tilt to 17° |
| `SOC=87%` | Battery at 87% |
| `RSSI=-89dBm` | 4G signal, above -90 is good |
| `conf=220` | Direction confidence 220/255 (>200 = very reliable, <50 = don't use) |
| `Faults: none` | No faults, everything OK |
| `DIR_OK CALIB` | Direction valid + calibrated |

> **If Faults appear**: Cross-reference with the fault code table in README. `LOW_BAT` = needs charging, `SENSOR_OPEN` = a sensor is loose or dirty.

---

## 9. Connect Solar Array Motors

`panel_az` and `panel_tilt` from the receiver are the angles the motors need to go to:

| Parameter | Physical Meaning | Motor Action |
|-----------|-----------------|--------------|
| `panel_az` | Horizontal rotation (0°=N, 90°=E, 180°=S) | **Azimuth motor** to this angle |
| `panel_tilt` | Tilt angle (0°=vertical, 90°=flat face-up) | **Elevation motor** to this angle |

### How to Connect

In `mqtt_receiver.py`, after the data is printed (around line 128), add your motor control code. Examples:

- **Serial stepper controller**: `serial.write(f"AZ={panel_az}\nTILT={panel_tilt}\n")`
- **Modbus RTU**: write holding registers 0x0000=panel_az, 0x0001=panel_tilt
- **CAN bus**: send standard frame ID=0x100, data=[az_hi, az_lo, tilt_hi, tilt_lo]

The specific protocol depends on your motor driver model (check the datasheet).

---

## 10. Daily Operation & Maintenance

### Normal Operation

The ball is fully autonomous:
- Every 5 seconds: wake → scan → diagnose → compute → display → publish → sleep
- Sleep current < 1 mA
- Solar panel charges battery during daylight; SOC stays 80–100%
- OLED shows status each wake cycle, then powers off to save energy

### What You Actually Need to Do

| Frequency | Task |
|-----------|------|
| Daily | Glance at receiver for red Faults |
| Monthly | Check that ball surface is clean |
| Every 6 months | Replace PTFE dust sock (~$1, see `hardware/dust_protection.md`) |
| Yearly | Renew IoT SIM data plan |

### Overcast / Night

The ball auto-detects conditions:
- **Overcast**: `FLAG_OVERCAST` set, direction defaults to zenith `(0,0,1)`, confidence reduced
- **Night**: `FLAG_NIGHT` set, direction defaults to zenith, confidence very low
- Use `conf` to decide whether to move the array (conf < 50 = better to stay put and save power)

---

## 11. Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|-------------|----------|
| No serial output | Wrong baud rate / charge-only USB cable | Use 115200 baud, swap cable for a data cable |
| All sensors read 0 | 3.3V rail missing / I2C not working | Check 3.3V with multimeter, verify I2C pull-up resistors |
| One sensor stuck max/min | Solder short or open on that channel | Continuity-test from sensor legs to MUX input with multimeter |
| 4G init fails | SIM not seated / no signal | Re-insert SIM, move to open area |
| Direction wrong after cal | Calibrated on cloudy day / ball not level | Re-calibrate on a clear day |
| MQTT won't publish | No network / broker unreachable | broker.emqx.io rate-limits sometimes; set up a local broker |
| OLED blank | I2C address conflict | Check OLED is 0x3C (not 0x3D) |
| Battery drains fast | Solar panel dirty / MT3608 output wrong | Clean panel, verify MT3608 output is exactly 5V |

---

> **Firmware version**: v1.1.0  
> **Last updated**: June 2026  
> **Full documentation**: See `hardware/`, `docs/`, `tests/` directories in the repository
