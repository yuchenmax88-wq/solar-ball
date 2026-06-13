"""
Unit tests for power.c — battery voltage to State-of-Charge calculation.
Ports the C linear interpolation logic to Python.
"""

import sys

# ---- Replicate C logic ----

BATTERY_FULL_MV = 4200
BATTERY_EMPTY_MV = 3200


def power_get_soc(millivolts: int) -> int:
    """Exact port of power_get_soc() from power.c"""
    mv = millivolts
    if mv >= BATTERY_FULL_MV:
        return 100
    elif mv <= BATTERY_EMPTY_MV:
        return 0
    else:
        # Linear interpolation: soc = 100 * (mv - empty) / (full - empty)
        soc = int(100 * (mv - BATTERY_EMPTY_MV) / (BATTERY_FULL_MV - BATTERY_EMPTY_MV))
        return soc


# ---- Tests ----

passed = 0
failed = 0


def check(name, condition, detail=""):
    global passed, failed
    if condition:
        passed += 1
    else:
        failed += 1
        print(f"  FAIL: {name} {detail}")


def test_full_battery():
    check("4200mV=100%", power_get_soc(4200) == 100)
    check("4300mV=100%", power_get_soc(4300) == 100)
    check("5000mV=100%", power_get_soc(5000) == 100)


def test_empty_battery():
    check("3200mV=0%", power_get_soc(3200) == 0)
    check("3100mV=0%", power_get_soc(3100) == 0)
    check("2800mV=0%", power_get_soc(2800) == 0)
    check("0mV=0%", power_get_soc(0) == 0)


def test_midpoints():
    # 3700mV = (3700-3200)/1000 * 100 = 50%
    check("3700mV=50%", power_get_soc(3700) == 50)
    # 3950mV = (3950-3200)/1000 * 100 = 75%
    check("3950mV=75%", power_get_soc(3950) == 75)
    # 3450mV = (3450-3200)/1000 * 100 = 25%
    check("3450mV=25%", power_get_soc(3450) == 25)


def test_typical_liion_voltages():
    """Standard 18650 voltage-to-SOC mapping sanity check"""
    # These are approximate real Li-ion voltages
    check("4200mV fully charged", power_get_soc(4200) == 100)
    check("4100mV ~90%", 85 <= power_get_soc(4100) <= 95, f"got {power_get_soc(4100)}%")
    check("3900mV ~70%", 65 <= power_get_soc(3900) <= 75, f"got {power_get_soc(3900)}%")
    check("3700mV ~50%", 45 <= power_get_soc(3700) <= 55, f"got {power_get_soc(3700)}%")
    check("3500mV ~30%", 25 <= power_get_soc(3500) <= 35, f"got {power_get_soc(3500)}%")
    check("3300mV ~10%", 5 <= power_get_soc(3300) <= 15, f"got {power_get_soc(3300)}%")


def test_monotonic():
    """SOC should be monotonically increasing with voltage"""
    prev = -1
    for mv in range(3000, 4300, 10):
        soc = power_get_soc(mv)
        if soc < prev:
            check(f"monotonic at {mv}mV", False, f"soc went {prev} -> {soc}")
            return
        prev = soc
    check("monotonic", True)


def test_no_overflow():
    """uint8_t SOC should always fit in 0-255 (C code uses uint8_t)"""
    for mv in range(0, 65000, 100):
        soc = power_get_soc(mv)
        if soc < 0 or soc > 255:
            check(f"overflow at {mv}mV", False, f"soc={soc}")
            return
    check("no_overflow", True)


def test_integer_division_truncation():
    """C integer division truncates toward zero. Verify our Python int() matches."""
    # (4199 - 3200) * 100 / 1000 = 99.9 -> C truncation -> 99
    check("4199mV=99%", power_get_soc(4199) == 99)
    # (3201 - 3200) * 100 / 1000 = 0.1 -> 0
    check("3201mV=0%", power_get_soc(3201) == 0)


# ---- Run ----

if __name__ == "__main__":
    print("=== power.c SOC calculation tests ===\n")

    test_full_battery()
    test_empty_battery()
    test_midpoints()
    test_typical_liion_voltages()
    test_monotonic()
    test_no_overflow()
    test_integer_division_truncation()

    print(f"\n--- Results: {passed} passed, {failed} failed ---")
    sys.exit(0 if failed == 0 else 1)
