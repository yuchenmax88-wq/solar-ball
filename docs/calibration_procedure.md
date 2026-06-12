# Calibration Procedure — Solar Ball v1.0

## Why Calibration Is Needed

The ball has 80 sensors wired to specific MUX+ADC channels (Physical Channels
0-79). After assembly, we may have mixed up which sensor is connected to which
channel. More importantly, even if the wiring is correct, each ALS-PT19 has
slightly different sensitivity. Calibration solves both problems:

1. **Channel-to-Position Mapping**: Determines which physical ADC channel
   corresponds to which position on the ball surface
2. **Baseline Normalization**: Measures each sensor's response to uniform
   light and creates normalization coefficients

## Calibration Method: Sequential Illumination

Each sensor hole on the ball is numbered (0-79, top to bottom).
We shine a bright light at each hole, one at a time, and record
which ADC channel lights up the most.

## Prerequisites

- Ball fully assembled and powered (USB serial connection to PC)
- Bright flashlight (smartphone LED flashlight works well)
- PC with Python 3 + `pyserial` installed:
  ```
  pip install -r tools/requirements.txt
  ```
- Know the ball's serial port (e.g., COM3 on Windows, /dev/ttyUSB0 on Linux)

## Steps

### 1. Enter Calibration Mode

1. Connect the ball to PC via USB-UART (use a USB-to-TTL adapter on the
   ESP32's UART0 pins, or the built-in USB port if using a dev board)
2. Open a serial monitor to verify the ball is running
3. **Hold the BOOT button (GPIO 0) while pressing RESET**, then release RESET,
   then release BOOT
4. The ball enters calibration mode and prints:
   ```
   === Solar Ball Auto-Calibration ===
   Ready to receive commands via UART.
   ```

### 2. Run the Calibration Tool

In another terminal window:

```
python tools/auto_calibrate.py COM3
```

### 3. Follow the Prompts

The tool will walk you through all 80 positions:

```
Position #0/79:
  Shine the flashlight directly at position #0 on the ball.
  Hold steady, then press [Enter]...
```

For each position:
1. Locate the position number on the ball (printed or labeled on the shell)
2. Hold the flashlight as close as possible (within 1cm) to the hole
3. Keep it steady and press Enter
4. Wait ~2 seconds for the scan
5. Verify the result shows a high value (e.g., >20000)

> **Tips**:
> - Work in a dim room for best contrast
> - If the value is low (<1000), the flashlight may be too far or angled
> - The phone LED works better than a regular flashlight due to focused beam
> - If ambient light is very bright (sunlight), cover unused areas with a cloth

### 4. Verification

After all 80 positions, the tool saves the mapping to NVS.

To verify: restart the ball (remove BOOT button) and check the serial log:
```
...
Channel mapping loaded from NVS
...
```

### 5. What If I Skip Calibration?

If no calibration data exists, the ball uses:
- **Identity mapping** (Physical Channel i = Position i)
- **Max-based normalization** (each sensor value / maximum across all sensors)

This works well enough if the wiring is correct. Direction accuracy will
still be within ~5-8°, just not the optimal ~3-5°.

## Baseline Calibration (Optional Second Step)

For the best accuracy, perform a baseline calibration after the mapping:

1. Place the ball in **uniform diffuse light** (overcast sky through a
   white diffuser, or an integrating sphere)
2. Open a serial connection and send:
   ```
   CAL:BASELINE_START
   ```
3. The ball scans all 80 sensors 100 times each and stores the average
   as the baseline reference

## Troubleshooting

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| "No strong reading!" | Flashlight too far | Hold within 1cm of hole |
| Multiple channels respond | Light bleeding through shell | Use opaque tape around the hole |
| Same channel every time | That sensor shorted/broken | Check solder joint |
| All channels read 0 | Power or I2C issue | Check 3.3V and ADS1115 wiring |
| "NVS write failed" | Flash storage full | Erase NVS: `idf.py erase-flash` |
