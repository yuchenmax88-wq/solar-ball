# Circuit Schematic — Solar Ball v1.0

## 1. Block Diagram

```
                     +---------------------------+
                     |  5V Flexible Solar Panel  |
                     +-------------+-------------+
                                   |
                            +------v------+
                            |   TP4056    |
                            | Li-ion Charger|
                            +------+------+
                                   |
                        +----------+----------+
                        |    18650 3.7V       |
                        |  (2000mAh + PCB)    |
                        +----------+----------+
                                   |
                            +------v------+
                            |   MT3608    |
                            | Boost to 5V |
                            +------+------+
                                   |
                         +---------+---------+
                         |                   |
                  +------v------+     +------v------+
                  |  AMS1117    |     |  SIM7600G   |
                  | 5V -> 3.3V  |     |  4G Module  |
                  +------+------+     |  5V input   |
                         |            +------+------+
                  +------v------+            |
                  |   ESP32     |<--- UART ---+
                  |  (3.3V)     |
                  |             |<--- I2C1 ---> ADS1115 #1 (0x48)
                  |             |<--- I2C2 ---> ADS1115 #2 (0x49)
                  |             |
                  |  GPIO[32-33] |---> MUX S0,S1 (shared)
                  |  GPIO[25-26] |---> MUX S2,S3 (shared)
                  |  GPIO[27]   |---> MUX EN0 (bank 0, sensors 0-15)
                  |  GPIO[14]   |---> MUX EN1 (bank 1, sensors 16-31)
                  |  GPIO[12]   |---> MUX EN2 (bank 2, sensors 32-47)
                  |  GPIO[13]   |---> MUX EN3 (bank 3, sensors 48-63)
                  |  GPIO[15]   |---> MUX EN4 (bank 4, sensors 64-79)
                  |  GPIO[36]   |<--- Battery voltage divider
                  +-------------+

## 2. Sensor Channel

Each of the 80 sensors:

```
       3.3V
         │
        ╱
    10kΩ R
        ╲
         │
         +─────── to MUX input (COM pin on CD74HC4067)
         │
       ┌─┴─┐
       │   │
       │ ALS│
       │ PT19 │
       │   │
       └─┬─┘
         │
        ═══ 100nF (optional, noise filtering)
         │
        GND
```

## 3. MUX Channel Mapping

| MUX Chip | Enable GPIO | ADS1115 | ADC Ch | Sensors | Channel Range |
|:--------:|:-----------:|:-------:|:------:|:-------:|:-------------:|
| #1 | GPIO 27 | #1 (0x48) | AIN0 | 0-15 | 0-15 |
| #2 | GPIO 14 | #1 (0x48) | AIN1 | 16-31 | 16-31 |
| #3 | GPIO 12 | #2 (0x49) | AIN0 | 32-47 | 32-47 |
| #4 | GPIO 13 | #2 (0x49) | AIN1 | 48-63 | 48-63 |
| #5 | GPIO 15 | #2 (0x49) | AIN2 | 64-79 | 64-79 |

## 4. Full GPIO Allocation

| Pin | Function | Direction | Notes |
|:---:|:---------|:---------:|:------|
| 21 | I2C1_SDA | I/O | ADS1115 #1 |
| 22 | I2C1_SCL | O | ADS1115 #1 |
| 18 | I2C2_SDA | I/O | ADS1115 #2 |
| 19 | I2C2_SCL | O | ADS1115 #2 |
| 32 | MUX_S0 | O | All MUX chips |
| 33 | MUX_S1 | O | All MUX chips |
| 25 | MUX_S2 | O | All MUX chips |
| 26 | MUX_S3 | O | All MUX chips |
| 27 | MUX_EN0 | O | MUX bank 0 (active low) |
| 14 | MUX_EN1 | O | MUX bank 1 (active low) |
| 12 | MUX_EN2 | O | MUX bank 2 (active low) |
| 13 | MUX_EN3 | O | MUX bank 3 (active low) |
| 15 | MUX_EN4 | O | MUX bank 4 (active low) |
| 17 | MODEM_TX | O | UART to SIM7600G |
| 16 | MODEM_RX | I | UART from SIM7600G |
| 23 | MODEM_PWR_KEY | O | Power-on pulse |
| 2 | LCD_MOSI | O | Sharp Memory LCD SPI data |
| 4 | LCD_SCLK | O | Sharp Memory LCD SPI clock |
| 5 | LCD_CS | O | Sharp Memory LCD SPI chip select |
| 36 | BAT_ADC | I | Battery voltage (ADC1_CH0) |
| 0 | BOOT | I | Calibration mode (active low) |
| 5V | MODEM_PWR | - | External 5V to SIM7600G |

## 5. Display (Sharp Memory LCD)
```
ESP32                  Sharp LS013B7DH03
GPIO 2 (MOSI) ──────── DIN  (SPI data)
GPIO 4 (SCLK) ──────── SCLK (SPI clock)
GPIO 5 (CS)   ──────── SCS  (chip select)
3.3V          ──────── VDD
GND           ──────── GND
```
Image persists during ESP32 deep sleep. 0 power to maintain.

## 5. Battery Voltage Divider

```
   Batt (+) ---- 100kΩ ----+---- 100kΩ ---- GND
                            |
                           ADC (GPIO 36)

   V_ADC = V_BAT × 100k / (100k + 100k) = V_BAT / 2
```

## 6. Wiring Notes

- I2C pull-up resistors: 4.7kΩ to 3.3V on both SDA and SCL lines
- SIM7600G power supply: must be 5V capable of 2A peak
- All MUX EN pins are ACTIVE LOW (0 = enabled, 1 = disabled)
- MUX S0-S3 are Schmitt-trigger inputs; direct GPIO connection is fine
- Ensure ALS-PT19 anode (long leg) connects to VCC through resistor
- Keep I2C and analog traces short to reduce noise
