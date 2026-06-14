# Contributing to Solar Ball

Thanks for your interest in contributing. This document outlines the workflow and conventions.

## Development Environment

### Prerequisites

- **Python 3.8+** — for tests, tools, and code generation
- **PlatformIO** — for firmware builds (`pip install platformio`)
- **ESP-IDF v5.x** — PlatformIO handles this automatically

### Setup

```bash
git clone https://github.com/yuchenmax88-wq/solar-ball.git
cd solar-ball

# Generate sensor coordinate header (required before first build)
cd firmware/main/scripts
python generate_sensor_coords.py

# Build firmware
cd ../firmware
platformio run
```

## Project Conventions

### Code Style

- **C** (firmware): ESP-IDF style. 4-space indentation, snake_case functions, descriptive variable names. Keep modules small (<400 lines per .c file).
- **Python** (tools/tests): PEP 8, 4-space indentation. Zero external dependencies for tests (stdlib only). Tools use `requirements.txt`.
- No comments unless explaining non-obvious behavior — the code should be self-documenting.

### Module Boundaries

Each `.c`/`.h` pair in `firmware/main/` is a self-contained module:

| Module | Responsibility |
|--------|---------------|
| `main.c` | Boot flow orchestration |
| `sensor_scan` | MUX + ADC I2C driver, sensor normalization |
| `direction` | Weighted centroid algorithm |
| `mqtt_4g` | SIM7600G AT commands, MQTT, HTTP |
| `power` | Battery ADC, SOC, deep sleep |
| `calibrate` | NVS, auto/manual calibration |
| `sun_calc` | NOAA solar ephemeris |
| `display` | Sharp Memory LCD SPI driver |
| `sensor_diag` | Fault detection, weather, confidence |
| `remote_diag` | Scan/publish counters |
| `ota` | OTA firmware upgrade |
| `remote_config` | Remote config via MQTT (NVS) |

When adding features, prefer new modules over expanding existing ones.

### Configuration

All hardware-specific settings go in `firmware/main/config.h`. No magic numbers in source files.

## Testing

### Run the full test suite

```bash
cd tests
python run_all.py
```

Expected output: 8 suites, all passing (418 assertions).

### Adding tests

1. Create `tests/test_<feature>.py`
2. Use the `unittest` standard library (no pytest, no external deps)
3. Import from `run_all.py` pattern — no test framework discovery magic
4. Register new test file in `run_all.py`
5. Tests must be deterministic (no random seeds, no network calls)

### Simulation testing

```bash
python tests/simulate.py                # random sun positions
python tests/simulate.py --scan         # full sky sweep
python tests/simulate.py --noiseless    # ideal sensor accuracy test
```

## Pull Request Process

1. **Open an issue first** for feature requests or bugs — this avoids wasted work
2. **Branch naming**: `feature/description` or `fix/description`
3. **Keep PRs small**: one feature or fix per PR
4. **Run tests before pushing**: `cd tests && python run_all.py`
5. **Update CHANGELOG.md** under the `[Unreleased]` section
6. **Update README.md** if public API, config, or tool usage changes
7. **Don't bump the version** — maintainers handle version tags

### Commit Messages

```
<module>: <imperative description>

- Bullet points for details if needed
```

Examples:
```
ota: add MQTT-triggered firmware upgrade via SIM7600G HTTP
display: fix memory LCD partial refresh on re-init
mqtt_4g: add chunked HTTP download for OTA support
```

## Release Process (for maintainers)

1. Move `[Unreleased]` entries to a dated version section in `CHANGELOG.md`
2. Bump `FIRMWARE_VERSION` in `firmware/main/config.h`
3. Update version in `README.md` badge
4. Tag the release:
   ```bash
   git tag -a v1.3.0 -m "Solar Ball v1.3.0 — OTA firmware upgrade"
   git push origin v1.2.0
   ```
5. Create a GitHub Release from the tag with the changelog entry

## Hardware Contributions

For hardware changes (schematic, BOM, 3D design):
- Discuss in an issue first
- Update `hardware/BOM.md` with cost delta
- Verify the pin map in `hardware/schematic.md` doesn't conflict with `config.h`

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
