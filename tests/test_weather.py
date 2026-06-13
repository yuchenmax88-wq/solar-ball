"""
Unit tests for sensor_diag.c weather classification logic.
Ports the C variance/overcast/night detection to Python.
"""
import math
import sys
import random

SENSOR_COUNT = 80

FAULT_NONE = 0x0000
FAULT_SENSOR_OPEN = 0x0001
FAULT_SENSOR_SHORT = 0x0002
FAULT_SENSOR_SATURATED = 0x0004
FAULT_OVERCAST_SKY = 0x0100
FAULT_NO_SIGNAL = 0x0200
FAULT_NO_CALIBRATION = 0x0080

FLAG_DIRECTION_VALID = 0x01
FLAG_OVERCAST = 0x02
FLAG_NIGHT = 0x04
FLAG_CALIBRATED = 0x08
FLAG_BASELINE_CAL = 0x10

OVERCAST_CV_THRESHOLD = 0.15
OPEN_THRESHOLD = int(0.90 * 65535)
SHORT_THRESHOLD = int(0.02 * 65535)
SATURATION_THRESHOLD = int(0.05 * 65535)


def sensor_diag_check_channel(raw_value, min_raw, max_raw):
    if raw_value >= OPEN_THRESHOLD:
        return FAULT_SENSOR_OPEN
    if raw_value <= SHORT_THRESHOLD:
        return FAULT_SENSOR_SHORT
    if raw_value <= SATURATION_THRESHOLD:
        return FAULT_SENSOR_SATURATED
    return 0


def sensor_diag_analyze(raw_values, adc_errors=0, has_calib=True, has_baseline=False):
    faults = FAULT_NONE
    conf = 255
    fl = 0
    fault_count = 0
    saturated_count = 0

    max_raw = max(raw_values) if raw_values else 0
    min_raw = min(raw_values) if raw_values else 0xFFFF

    for raw in raw_values:
        fault = sensor_diag_check_channel(raw, min_raw, max_raw)
        if fault in (FAULT_SENSOR_OPEN, FAULT_SENSOR_SHORT):
            faults |= fault
            fault_count += 1
        if fault == FAULT_SENSOR_SATURATED:
            faults |= FAULT_SENSOR_SATURATED
            saturated_count += 1

    if adc_errors > 0:
        faults |= 0x0008  # FAULT_ADC_ERROR
        conf -= min(adc_errors * 5, 50)

    # Variance check
    mean = sum(raw_values) / len(raw_values)
    variance = sum((r - mean) ** 2 for r in raw_values) / len(raw_values)

    if max_raw > 0 and min_raw < max_raw:
        if mean > 0.1:
            cv = math.sqrt(variance) / mean
            if cv < OVERCAST_CV_THRESHOLD:
                faults |= FAULT_OVERCAST_SKY
                fl |= FLAG_OVERCAST
                conf = int(conf * 0.4)

    # Night detection
    if max_raw > 0:
        brightness = 1.0 - float(min_raw) / float(max_raw)
        if brightness < 0.01 and max_raw > int(OPEN_THRESHOLD * 0.8):
            faults |= FAULT_NO_SIGNAL
            fl |= FLAG_NIGHT
            conf = int(conf * 0.1)

    if has_calib:
        fl |= FLAG_CALIBRATED
    else:
        faults |= FAULT_NO_CALIBRATION
    if has_baseline:
        fl |= FLAG_BASELINE_CAL

    if fault_count > 0:
        conf = max(10, int(conf * (1.0 - fault_count * 0.05)))

    if fault_count < 3:
        fl |= FLAG_DIRECTION_VALID

    return faults, conf, fl


# ---- Tests ----
passed = 0
failed = 0

def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
    else:
        failed += 1
        print(f"  FAIL: {name} {detail}")


def test_clear_sky():
    """Daytime sunny: high variance between bright and dark sensors"""
    # Bright sensors at ~5000 (above SHORT threshold), dark at ~50000
    raw = [int(5000 + random.randint(0, 1000)) for _ in range(20)] + \
          [int(50000 - random.randint(0, 5000)) for _ in range(60)]
    faults, conf, fl = sensor_diag_analyze(raw)
    check("clear sky not overcast", not (faults & FAULT_OVERCAST_SKY))
    check("clear sky not night", not (faults & FAULT_NO_SIGNAL))
    check("clear sky high confidence", conf > 150, f"conf={conf}")
    check("clear sky direction valid", bool(fl & FLAG_DIRECTION_VALID))


def test_overcast_sky():
    """Overcast: all sensors similar mid-range values (low variance)"""
    raw = [30000 + random.randint(-500, 500) for _ in range(SENSOR_COUNT)]
    faults, conf, fl = sensor_diag_analyze(raw)
    check("overcast detected", bool(faults & FAULT_OVERCAST_SKY))
    check("overcast flag set", bool(fl & FLAG_OVERCAST))
    check("overcast low confidence", conf < 130)


def test_nighttime():
    """Night: all sensors near max raw (dark), low brightness ratio"""
    raw = [60000 + random.randint(-200, 200) for _ in range(SENSOR_COUNT)]
    faults, conf, fl = sensor_diag_analyze(raw)
    check("night detected", bool(faults & FAULT_NO_SIGNAL))
    check("night flag set", bool(fl & FLAG_NIGHT))
    check("night very low confidence", conf < 100)


def test_sensor_open():
    raw = [1000] * SENSOR_COUNT
    raw[5] = OPEN_THRESHOLD + 1
    faults, conf, fl = sensor_diag_analyze(raw)
    check("open detected", bool(faults & FAULT_SENSOR_OPEN))


def test_sensor_short():
    raw = [30000] * SENSOR_COUNT
    raw[7] = SHORT_THRESHOLD - 1
    faults, conf, fl = sensor_diag_analyze(raw)
    check("short detected", bool(faults & FAULT_SENSOR_SHORT))


def test_sensor_saturated():
    raw = [30000] * SENSOR_COUNT
    raw[3] = SATURATION_THRESHOLD - 1
    faults, conf, fl = sensor_diag_analyze(raw)
    check("saturation detected", bool(faults & FAULT_SENSOR_SATURATED))


def test_multiple_faults():
    raw = [30000] * SENSOR_COUNT
    raw[0] = OPEN_THRESHOLD + 100
    raw[1] = SHORT_THRESHOLD - 100
    raw[2] = SATURATION_THRESHOLD - 100
    faults, conf, fl = sensor_diag_analyze(raw)
    check("multi-fault open", bool(faults & FAULT_SENSOR_OPEN))
    check("multi-fault short", bool(faults & FAULT_SENSOR_SHORT))
    check("multi-fault saturation", bool(faults & FAULT_SENSOR_SATURATED))
    check("multi-fault confidence reduced", conf < 235, f"conf={conf}")


def test_calibration_flags():
    raw = [30000] * SENSOR_COUNT
    _, _, fl1 = sensor_diag_analyze(raw, has_calib=False, has_baseline=False)
    check("no calib flag absent", not (fl1 & FLAG_CALIBRATED))
    check("no baseline flag absent", not (fl1 & FLAG_BASELINE_CAL))

    _, _, fl2 = sensor_diag_analyze(raw, has_calib=True, has_baseline=True)
    check("calib flag present", fl2 & FLAG_CALIBRATED)
    check("baseline flag present", fl2 & FLAG_BASELINE_CAL)


def test_all_zero_edge_case():
    """All zeros: all sensors shorted, no crash"""
    raw = [0] * SENSOR_COUNT
    faults, conf, fl = sensor_diag_analyze(raw)
    check("all zero no crash", True)
    check("all zero shorts detected", bool(faults & FAULT_SENSOR_SHORT))
    check("all zero saturated NOT set (short takes priority)", not (faults & FAULT_SENSOR_SATURATED))
    check("all zero confidence very low", conf < 30, f"conf={conf}")
    check("all zero direction INVALID", not (fl & FLAG_DIRECTION_VALID))


# ---- Run ----
if __name__ == "__main__":
    print("=== sensor_diag weather classification tests ===\n")
    random.seed(42)
    test_clear_sky()
    test_overcast_sky()
    test_nighttime()
    test_sensor_open()
    test_sensor_short()
    test_sensor_saturated()
    test_multiple_faults()
    test_calibration_flags()
    test_all_zero_edge_case()
    print(f"\n--- Results: {passed} passed, {failed} failed ---")
    sys.exit(0 if failed == 0 else 1)
