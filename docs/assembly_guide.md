# Assembly Guide — Solar Ball v1.0

## Tools Required

- Soldering iron (temperature-controlled, 350°C)
- Fine-tip tweezers
- Multimeter (continuity + voltage)
- Hot glue gun
- Small Phillips screwdriver
- Wire stripper / cutter
- Heat shrink tubing (various sizes)
- Isopropyl alcohol + lint-free wipes

## Step 1: PCB Assembly (or Perf-board Wiring)

**Option A: Custom PCB (Recommended)**
1. Order PCB from JLCPCB or similar using the included Gerber files
2. Solder components in order: resistors first, then IC sockets, then connectors
3. Inspect all solder joints under magnification

**Option B: Hand-wiring (Prototype)**
1. Use two 6×9cm perf-boards
2. Mount 5 CD74HC4067 chips on one board (the "MUX board")
3. Mount ESP32, ADS1115 modules, power modules on the other ("main board")
4. Wire all 80 ALS-PT19 leads to MUX input pins via IDC connectors

## Step 2: Sensor Wiring

For each of the 80 ALS-PT19 sensors:
1. Cut two wires: 10cm each (one for VCC+resistor, one for MUX input)
2. Solder a 10kΩ resistor between ALS-PT19 anode and VCC (3.3V)
3. Solder ALS-PT19 cathode to GND
4. Connect the anode+resistor junction to the appropriate MUX input pin
5. Add heat shrink over exposed leads

**Critical alignment**: Each sensor's lead must be labeled with its
MUX bank and channel number (e.g., "B2-C7" = MUX bank 2, channel 7).
This ensures you know which PHYSICAL CHANNEL each sensor is connected to.

## Step 3: Sensor Installation in Ball Shell

1. Identify Position #0 (top of ball) on the shell
2. Apply a small dab of hot glue around the inside of each sensor hole
3. Insert each sensor from the INSIDE of the ball, with the phototransistor
   lens flush with the outer surface
4. The sensor labeling MUST correspond to the hole number:
   - Physical Channel #0 → Position #0 (top)
   - Physical Channel #1 → Position #1
   - ... and so on through Position #79

   **However**, if you mix up the wiring (which is likely with 80 sensors),
   don't worry — the auto-calibration tool will fix the mapping.

## Step 4: Internal Frame Assembly

1. Mount the MUX board to the bottom of the internal frame
2. Mount the main board above the MUX board (stacked)
3. Secure the 18650 battery in the battery holder
4. Mount the SIM7600G in its slot (ensure antenna connector is accessible)
5. Connect all inter-board wiring harnesses
6. Connect 4G antenna (SMA to IPEX cable)

## Step 5: Power System

1. Connect TP4056 input to the solar panel leads
2. Connect TP4056 output to 18650 battery (observe polarity!)
3. Connect 18650 to MT3608 input
4. Set MT3608 output to 5.0V (adjust potentiometer while measuring)
5. Connect MT3608 5V output to:
   - AMS1117 input (for 3.3V to ESP32)
   - SIM7600G 5V input pin
6. Connect AMS1117 output to ESP32 3.3V pin

**WARNING**: Triple-check polarity on ALL connections before connecting the battery.
Reverse polarity will destroy the electronics.

## Step 6: Power-On Test

1. Leave the ball shell open
2. Connect the battery (or USB to TP4056)
3. The ESP32 should boot (check serial output at 115200 baud)
4. Verify ESP32 console prints the boot banner
5. Verify ADS1115 I2C detection (scan for 0x48 and 0x49)
6. Verify SIM7600G responds to AT commands

## Step 7: Auto-Calibration

After assembly, run the auto-calibration tool:

```
python tools/auto_calibrate.py COM3
```

See `docs/calibration_procedure.md` for detailed instructions.

## Step 8: Final Sealing

1. Once calibration is complete and verified:
2. Apply silicone sealant to the equator joint of the ball shell
3. Apply silicone sealant around each sensor hole (inside)
4. Seal the bottom access opening
5. Allow 24 hours for silicone to cure
6. Mount the ball on the support pole
7. Secure the solar panel on top

## Step 9: Field Deployment

1. Install the support pole at the center of the solar array
2. Position the ball at ~2m height (above the panels' maximum tilt angle)
3. Ensure the top of the ball points straight up (verify with bubble level)
4. Insert IoT SIM card
5. Power on the system
6. Verify MQTT messages on the broker within 10 seconds of boot
