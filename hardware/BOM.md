# Bill of Materials — Solar Ball v1.0

## Core Electronics

| # | Component | Specification | Qty | Unit Price | Total |
|---|-----------|--------------|:---:|:----------:|:-----:|
| 1 | ESP32-WROOM-32 | Development board (38-pin) | 1 | ¥20 | ¥20 |
| 2 | ALS-PT19 | Ambient light phototransistor, 570nm peak | 80 | ¥0.5 | ¥40 |
| 3 | CD74HC4067 | 16-channel analog multiplexer, DIP-24 | 5 | ¥3 | ¥15 |
| 4 | ADS1115 | 16-bit ADC, I2C, 4-channel, MSOP-10 | 2 | ¥8 | ¥16 |
| 5 | SIM7600G-H | 4G Cat-4 module, PCIe form factor | 1 | ¥80 | ¥80 |
| 6 | 18650 Li-ion Battery | 3.7V, 2000mAh, with protection PCB | 1 | ¥15 | ¥15 |
| 7 | TP4056 Module | 1A Li-ion charger, Micro-USB input | 1 | ¥3 | ¥3 |
| 8 | MT3608 Module | Boost converter, 2A max, 5V output | 1 | ¥3 | ¥3 |
| 9 | AMS1117-3.3 | 3.3V LDO regulator | 1 | ¥1 | ¥1 |

## Passives & Connectors

| # | Component | Specification | Qty | Unit Price | Total |
|---|-----------|--------------|:---:|:----------:|:-----:|
| 10 | Resistor 10kΩ | 0805 / DIP, 5% | 80 | ¥0.1 | ¥8 |
| 11 | Resistor 100kΩ | 0805, 1% (battery divider) | 2 | ¥0.1 | ¥0.2 |
| 12 | Resistor 4.7kΩ | 0805, I2C pull-up | 2 | ¥0.1 | ¥0.2 |
| 13 | Capacitor 100nF | 0805, decoupling | 10 | ¥0.1 | ¥1 |
| 14 | Capacitor 10μF | 0805, electrolytic | 5 | ¥0.3 | ¥1.5 |
| 15 | Pin headers | 2.54mm male, various | 1 pkg | ¥2 | ¥2 |

## Antenna & SIM

| # | Component | Specification | Qty | Unit Price | Total |
|---|-----------|--------------|:---:|:----------:|:-----:|
| 16 | 4G Antenna | SMA male, 3-5dBi, 700-2700MHz, magnetic base | 1 | ¥15 | ¥15 |
| 17 | SMA to IPEX cable | U.FL/IPEX to SMA pigtail | 1 | ¥5 | ¥5 |
| 18 | IoT SIM Card | China Mobile OneLink, 10MB/month | 1 | ¥60/yr | ¥60 |

## Power & Enclosure

| # | Component | Specification | Qty | Unit Price | Total |
|---|-----------|--------------|:---:|:----------:|:-----:|
| 19 | Flexible Solar Panel | 5V, 5W, ETFE, 180×130mm | 1 | ¥25 | ¥25 |
| 20 | 3D Printed Ball Shell | PETG, 100mm dia, 2mm wall, 80 holes | 1 | ¥30 | ¥30 |
| 21 | Internal Frame | 3D printed PLA, sensor mounting | 1 | ¥10 | ¥10 |
| 22 | Stainless Steel Pole | 25mm dia, 1.5m length | 1 | ¥15 | ¥15 |
| 23 | Silicone Sealant | Waterproof seal | 1 tube | ¥8 | ¥8 |
| 24 | PCB (custom) | 80×80mm, 2-layer, FR4, HASL | 1 | ¥20 | ¥20 |

## Subtotal

| Category | Total |
|----------|:-----:|
| Electronics | ¥196.2 |
| Passives | ¥12.7 |
| Antenna & SIM | ¥80 |
| Enclosure & Structure | ¥83 |
| **Grand Total** | **¥371.9** |

## Notes
- Prices are estimates from LCSC / Taobao / 1688 in CNY (2026)
- Excludes: soldering tools, multimeter, test equipment, shipping
- The custom PCB can be replaced with prototype board if hand-wiring 80 sensors; expect ~4-6 hours assembly time
