# Changelog

All notable changes to Solar Ball will be documented in this file.

## [1.3.0] — 2026-06-14

### Added
- **Data visualization** — Chart.js trajectory, telemetry, confidence, and fault timeline charts in web dashboard
- **Remote configuration over MQTT** — change ball_id, APN, broker, scan interval, battery thresholds without reflashing
- **Config wizard** — web-based `config.h` generator in dashboard
- **Fault root cause analysis** (`sensor_diag_deep_analyze`) — human-readable diagnostic reports with channel-level details
- **Calibration quality assessment** — `auto_calibrate.py --report` generates accuracy grade + coverage + gap analysis
- **Architecture documentation** (`docs/architecture.md`) — full system design, data flow, module dependency graph
- `remote_config.h/.c` module with NVS-backed persistent config via MQTT

### Changed
- Firmware version bumped to 1.3.0
- Dashboard: added **Charts** tab (trajectory, SOC/RSSI, confidence, fault timeline) + **Config Wizard** tab
- `sensor_diag.h/.c`: added `sensor_diag_deep_analyze()` with root cause text output
- `main.c`: integrated deep diagnostics + remote config handling in MQTT loop
- `CMakeLists.txt`: added `remote_config.c`
- `tools/auto_calibrate.py`: added `--report` flag with quality grading

## [1.2.0] — 2026-06-13

### Added
- **OTA (Over-The-Air) firmware upgrade** via MQTT + HTTP download through SIM7600G 4G modem
- MQTT command subscription on `/solar/ball/{id}/cmd` topic for OTA triggers
- OTA status tracking, progress reporting, and rollback-on-failure
- NVS-persisted OTA version validation
- Chunked HTTP download through modem AT commands (`ota_chunk_callback_t`)
- OTA module (`ota.h` / `ota.c`) with full `esp_ota` API integration
- OTA-specific config defines (`OTA_HTTP_MAX_RETRIES`, `OTA_FW_URL_PREFIX`, etc.)
- MQTT `ota_cmd_packet_t` and `ota_status_packet_t` protocol types
- `app_update` component added to ESP-IDF build requirements

### Changed
- Firmware version bumped from 1.1.0 to 1.2.0
- `main.c`: OTA init + MQTT subscribe + command polling added to boot flow
- `mqtt_4g.h/.c`: Added `mqtt_subscribe_cmd()`, `mqtt_poll_message()`, `modem_http_download()`, `modem_http_download_chunked()`, `modem_http_head_size()`
- `mqtt_protocol.h`: Added OTA command/status structs and command type defines
- `config.h`: Added OTA configuration section
- `CMakeLists.txt`: Added `ota.c` source and `app_update` component

## [1.1.0] — 2026-04-15

### Added
- MQTT protocol v1.1 with `err`, `conf`, and `flags` fields in direction payload
- Sensor fault detection (open/short/saturated) in `sensor_diag.h/.c`
- Weather classification (clear/overcast/night)
- Direction confidence scoring (0–255)
- Remote diagnostic counters (`remote_diag.h/.c`)
- Sharp Memory LCD display support (`display.h/.c`)
- Overcast/night safety: auto-point zenith with low confidence
- Fault bitmask in MQTT payload (10 bits)
- MQTT receiver decodes v1.1 fields with panel angle output

### Changed
- Direction packet struct expanded with `error_mask`, `confidence`, `flags`
- MQTT protocol updated for backward-compatible v1.1
- `sensor_scan` updated to provide raw ADC values for diagnostics
- `main.c` flow updated: scan + diag → compute → display → publish

## [1.0.0] — 2026-03-01

### Added
- Initial release
- 80-sensor Fibonacci sphere scan with 8x oversampling
- Weighted centroid direction algorithm (`direction.h/.c`)
- SIM7600G 4G MQTT publish every 5 seconds (`mqtt_4g.h/.c`)
- Self-powered (solar panel + 18650 Li-ion, 3-day battery)
- Sun auto-calibration via solar ephemeris (`calibrate.h/.c`, `sun_calc.h/.c`)
- Baseline normalization (uniform light reference)
- Manual flashlight calibration (80-step guided)
- NVS persistent storage for calibration data
- PC calibration tool (`tools/auto_calibrate.py`) — serial UART, manual + auto modes
- MQTT receiver tool (`tools/mqtt_receiver.py`)
- Desktop simulator (`tests/simulate.py`) with noise modeling
- Full test suite (7 suites, 389 assertions)
- GitHub Actions CI/CD (Python tests + firmware build)
- Hardware documentation (BOM, schematic, ball design, dust protection)
- Chinese and English operation manuals

[1.3.0]: https://github.com/yuchenmax88-wq/solar-ball/releases/tag/v1.3.0
[1.2.0]: https://github.com/yuchenmax88-wq/solar-ball/releases/tag/v1.2.0
[1.1.0]: https://github.com/yuchenmax88-wq/solar-ball/releases/tag/v1.1.0
[1.0.0]: https://github.com/yuchenmax88-wq/solar-ball/releases/tag/v1.0.0
