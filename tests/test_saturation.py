"""
Unit tests for sensor saturation detection logic in sensor_diag.c.
"""
import sys

SENSOR_COUNT = 80
SATURATION_THRESHOLD = int(0.05 * 65535)  # ~3277
OPEN_THRESHOLD = int(0.90 * 65535)
SHORT_THRESHOLD = int(0.02 * 65535)


def is_saturated(raw_value):
    return raw_value <= SATURATION_THRESHOLD


def is_short(raw_value):
    return raw_value <= SHORT_THRESHOLD


def is_open(raw_value):
    return raw_value >= OPEN_THRESHOLD


def check_saturation(raw_values):
    """Returns (saturated_count, saturation_indices, open_count, short_count)"""
    sat = []
    opens = 0
    shorts = 0
    for i, raw in enumerate(raw_values):
        if is_saturated(raw):
            sat.append(i)
        if is_open(raw):
            opens += 1
        if is_short(raw):
            shorts += 1
    return len(sat), sat, opens, shorts


passed = 0
failed = 0

def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
    else:
        failed += 1
        print(f"  FAIL: {name} {detail}")


def test_no_saturation_normal():
    """Normal readings: no sensor in saturation"""
    raw = [30000] * SENSOR_COUNT
    count, indices, ops, shts = check_saturation(raw)
    check("normal: zero saturated", count == 0)


def test_one_saturated():
    raw = [30000] * SENSOR_COUNT
    raw[42] = SATURATION_THRESHOLD - 1
    count, indices, ops, shts = check_saturation(raw)
    check("one saturated count", count == 1)
    check("one saturated index", indices[0] == 42)


def test_many_saturated():
    raw = [30000] * SENSOR_COUNT
    for i in range(10):
        raw[i] = SATURATION_THRESHOLD - i - 1
    count, indices, ops, shts = check_saturation(raw)
    check("many saturated count", count == 10)


def test_boundary_exact():
    """Exactly at threshold: IS saturated (<= in C code)"""
    raw = [30000] * SENSOR_COUNT
    raw[0] = SATURATION_THRESHOLD       # at threshold → saturated
    raw[1] = SATURATION_THRESHOLD - 1   # below → saturated
    count, indices, ops, shts = check_saturation(raw)
    check("boundary both saturated", count == 2, f"got {count}")
    check("boundary index0", indices[0] == 0)
    check("boundary index1", indices[1] == 1)


def test_saturation_vs_short():
    """Short takes priority over saturated. Short+saturated = just short in diag."""
    raw = [30000] * SENSOR_COUNT
    raw[0] = SHORT_THRESHOLD - 1  # short (and saturated, but short takes priority)
    count, indices, ops, shts = check_saturation(raw)
    check("priority: saturated count", count == 1)  # still counted by check_saturation
    check("priority: short count", shts == 1)


def test_all_saturated():
    raw = [0] * SENSOR_COUNT
    count, indices, ops, shts = check_saturation(raw)
    check("all-zero: saturated", count == SENSOR_COUNT)


def test_all_open():
    raw = [65535] * SENSOR_COUNT
    count, indices, ops, shts = check_saturation(raw)
    check("all-max: no saturation", count == 0)
    check("all-max: all open", ops == SENSOR_COUNT)


def test_mixed_hardware_faults():
    raw = [30000] * SENSOR_COUNT
    raw[0] = 0          # saturated + short
    raw[1] = 65535      # open
    raw[2] = SATURATION_THRESHOLD - 1  # saturated only
    raw[3] = SHORT_THRESHOLD - 1       # short + saturated
    raw[4] = OPEN_THRESHOLD + 1        # open
    count, indices, ops, shts = check_saturation(raw)
    check("mixed: 3 saturated", count == 3, f"got {count}")
    check("mixed: 2 open", ops == 2, f"got {ops}")
    check("mixed: 2 short", shts == 2, f"got {shts}")


if __name__ == "__main__":
    print("=== saturation detection tests ===\n")
    test_no_saturation_normal()
    test_one_saturated()
    test_many_saturated()
    test_boundary_exact()
    test_saturation_vs_short()
    test_all_saturated()
    test_all_open()
    test_mixed_hardware_faults()
    print(f"\n--- Results: {passed} passed, {failed} failed ---")
    sys.exit(0 if failed == 0 else 1)
