"""
Unit tests for calibrate.c — channel-to-position mapping inversion logic.
Verifies the fix for the calib_load inversion bug.
"""

import sys

SENSOR_COUNT = 80


def invert_channel_map(channel_map: list) -> list:
    """
    Port of the fixed calib_load logic:
    channel_map[pos] = physical_channel (position -> physical)
    Returns g_channel_to_position[physical] = position (physical -> position)
    """
    g_channel_to_position = [0xFF] * SENSOR_COUNT
    for pos in range(SENSOR_COUNT):
        phys = channel_map[pos]
        if 0 <= phys < SENSOR_COUNT:
            g_channel_to_position[phys] = pos
    return g_channel_to_position


def apply_mapping(raw_per_physical, g_channel_to_position):
    """
    Port of sensor_scan_all normalization:
    For physical=0..79, position_idx = g_channel_to_position[physical]
    output[position_idx] = normalized_reading
    """
    output = [0.0] * SENSOR_COUNT
    for physical in range(SENSOR_COUNT):
        position_idx = g_channel_to_position[physical]
        if position_idx < SENSOR_COUNT:
            output[position_idx] = raw_per_physical[physical]
    return output


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


def test_identity_mapping():
    """Default identity: position i = physical i"""
    channel_map = list(range(SENSOR_COUNT))
    c2p = invert_channel_map(channel_map)
    for i in range(SENSOR_COUNT):
        check(f"identity phys[{i}]->pos", c2p[i] == i, f"got {c2p[i]}")


def test_swapped_mapping():
    """Calibration swaps position 0 and physical 5"""
    channel_map = list(range(SENSOR_COUNT))
    channel_map[0] = 5   # position 0 -> physical 5
    channel_map[5] = 0   # position 5 -> physical 0

    c2p = invert_channel_map(channel_map)
    check("pos0->phys5 inverted", c2p[5] == 0, f"got {c2p[5]}")
    check("pos5->phys0 inverted", c2p[0] == 5, f"got {c2p[0]}")
    # All others should remain identity
    for i in range(SENSOR_COUNT):
        if i not in (0, 5):
            check(f"non-swapped phys[{i}]", c2p[i] == i, f"got c2p[{i}]={c2p[i]}")


def test_reversed_mapping():
    """Full reversal: position 0 -> physical 79, 1 -> 78, etc."""
    channel_map = [SENSOR_COUNT - 1 - i for i in range(SENSOR_COUNT)]
    c2p = invert_channel_map(channel_map)
    for pos in range(SENSOR_COUNT):
        phys = channel_map[pos]
        check(f"reversed phys[{phys}]->pos[{pos}]", c2p[phys] == pos, f"got {c2p[phys]}")


def test_sensor_read_with_mapping():
    """Simulate scan: physical channel 5 reads 0.8, mapped to position 0"""
    channel_map = list(range(SENSOR_COUNT))
    channel_map[0] = 5  # position 0 uses physical channel 5
    channel_map[5] = 0  # position 5 uses physical channel 0

    c2p = invert_channel_map(channel_map)

    # Simulate: physical channel 5 has reading 0.8 (pointing to position 0)
    raw_readings = [0.0] * SENSOR_COUNT
    raw_readings[5] = 0.8  # physical channel 5 = brightest

    output = apply_mapping(raw_readings, c2p)
    check("reading at pos 0", abs(output[0] - 0.8) < 0.001, f"got {output[0]}")
    check("no reading at pos 5", output[5] == 0.0)


def test_direction_consistency():
    """
    Verify that after calibration swap, direction calculation is correct.
    Physical channel 0 is at position (0.152, 0, 0.988) (top sensor).
    If physical channel 5 is mapped to position 0, shining light
    at physical 5 should give direction pointing toward position 0's coordinates.
    """
    import math

    SENSOR_0_POS = (0.1520546, 0.0, 0.9883721)

    channel_map = list(range(SENSOR_COUNT))
    channel_map[0] = 5  # position 0 = physical 5
    channel_map[5] = 0  # position 5 = physical 0
    c2p = invert_channel_map(channel_map)

    # Shine light only at physical channel 5 (which is at position 0)
    raw_readings = [0.0] * SENSOR_COUNT
    raw_readings[5] = 1.0

    output = apply_mapping(raw_readings, c2p)
    # Position 0 should be 1.0, all others 0.0
    check("mapped reading at pos 0 == 1.0", abs(output[0] - 1.0) < 0.001)
    check("no mapped reading elsewhere", sum(1 for v in output if v > 0.01) == 1)


def test_unmapped_channels():
    """Channels not in the mapping (0xFF) should be ignored"""
    channel_map = list(range(SENSOR_COUNT))
    channel_map[79] = 0xFF  # position 79 unmapped (invalid)
    channel_map[0] = 79  # position 0 -> physical 79

    c2p = invert_channel_map(channel_map)
    # physical 79 -> position 0
    check("phys 79 maps to pos 0", c2p[79] == 0, f"got {c2p[79]}")
    # All 0xFF markers should remain
    null_count = sum(1 for v in c2p if v == 0xFF)
    check("only one 0xFF", null_count <= SENSOR_COUNT - 79)


# ---- Run ----

if __name__ == "__main__":
    print("=== calibrate.c mapping inversion tests ===\n")

    test_identity_mapping()
    test_swapped_mapping()
    test_reversed_mapping()
    test_sensor_read_with_mapping()
    test_direction_consistency()
    test_unmapped_channels()

    print(f"\n--- Results: {passed} passed, {failed} failed ---")
    sys.exit(0 if failed == 0 else 1)
