"""
Unit tests for direction.c — weighted centroid direction algorithm.
Ports the C logic to Python for independent verification.
"""

import math
import sys
from typing import List, Tuple

# ---- Replicate C algorithm exactly ----

SENSOR_COUNT = 80
Z_EXCLUDE_THRESHOLD = -0.8660254  # cos(150deg)

# Fibonacci sphere positions (same as sensor_positions.h)
SENSOR_POSITIONS = [
    (0.1520546, 0.0000000, 0.9883721),
    (-0.1930591, -0.1768580, 0.9651163),
    (0.0293754, 0.3347177, 0.9418605),
    (0.2404415, -0.3136134, 0.9186047),
    (-0.4385574, 0.0775746, 0.8953488),
    (0.4128833, 0.2626426, 0.8720930),
    (-0.1372409, -0.5105294, 0.8488372),
    (-0.2600820, 0.5007721, 0.8255814),
    (0.5606683, -0.2047552, 0.8023256),
    (-0.5795066, -0.2392119, 0.7790698),
    (0.2775286, 0.5930625, 0.7558140),
    (0.2037239, -0.6495038, 0.7325581),
    (-0.6098912, 0.3534444, 0.7093023),
    (0.7105880, 0.1562208, 0.6860465),
    (-0.4306596, -0.6125690, 0.6627907),
    (-0.0987941, 0.7623876, 0.6395349),
    (0.6021824, -0.5075199, 0.6162791),
    (-0.8044977, -0.0332685, 0.5930233),
    (0.5825202, 0.5796855, 0.5697674),
    (-0.0386831, -0.8365576, 0.5465116),
    (-0.5459967, 0.6542866, 0.5232558),
    (0.8582913, -0.1154819, 0.5000000),
    (-0.7215690, -0.5020489, 0.4767442),
    (0.1956154, 0.8695303, 0.4534884),
    (0.4488142, -0.7832406, 0.4302326),
    (-0.8702263, 0.2776259, 0.4069767),
    (0.8382989, 0.3873154, 0.3837209),
    (-0.3601072, -0.8604578, 0.3604651),
    (-0.3186290, 0.8858699, 0.3372093),
    (0.8404328, -0.4417079, 0.3139535),
    (-0.9252114, -0.2438827, 0.2906977),
    (0.5211372, 0.8104881, 0.2674419),
    (0.1642489, -0.9557173, 0.2441860),
    (-0.7710866, 0.5971728, 0.2209302),
    (0.9769208, 0.0809359, 0.1976744),
    (-0.6686701, -0.7228128, 0.1744186),
    (0.0048222, 0.9884971, 0.1511628),
    (0.6663664, -0.7345717, 0.1279070),
    (-0.9902651, 0.0917775, 0.1046512),
    (0.7939164, 0.6025542, 0.0813953),
    (-0.1786808, -0.9821878, 0.0581395),
    (-0.5322814, 0.8458484, 0.0348837),
    (0.9643720, -0.2642944, 0.0116279),
    (-0.8896262, -0.4565415, -0.0116279),
    (0.3474053, 0.9370660, -0.0348837),
    (0.3764049, -0.9246292, -0.0581395),
    (-0.9006574, 0.4268384, -0.0813953),
    (0.9503655, 0.2930078, -0.1046512),
    (-0.5014693, -0.8556684, -0.1279070),
    (-0.2075398, 0.9664766, -0.1511628),
    (0.8027506, -0.5702364, -0.1744186),
    (-0.9727424, -0.1212310, -0.1976744),
    (0.6321530, 0.7426792, -0.2209302),
    (0.0353400, -0.9690842, -0.2441860),
    (-0.6763458, 0.6863172, -0.2674419),
    (0.9555667, -0.0488588, -0.2906977),
    (-0.7319221, -0.6047505, -0.3139535),
    (0.1300869, 0.9323987, -0.3372093),
    (0.5289947, -0.7682640, -0.3604651),
    (-0.8999329, 0.2070725, -0.3837209),
    (0.7947481, 0.4502725, -0.4069767),
    (-0.2785597, -0.8586643, -0.4302326),
    (-0.3698641, 0.8108939, -0.4534884),
    (0.8092276, -0.3433157, -0.4767442),
    (-0.8163358, -0.2891294, -0.5000000),
    (0.4001335, 0.7523939, -0.5232558),
    (0.2095053, -0.8108222, -0.5465116),
    (-0.6890667, 0.4478305, -0.5697674),
    (0.7942074, 0.1325064, -0.5930233),
    (-0.4852373, -0.6202780, -0.6162791),
    (-0.0597347, 0.7664378, -0.6395349),
    (0.5471841, -0.5111732, -0.6627907),
    (-0.7275231, 0.0070984, -0.6860465),
    (0.5243954, 0.4710623, -0.7093023),
    (-0.0661241, -0.6774852, -0.7325581),
    (-0.3933087, 0.5235012, -0.7558140),
    (0.6162588, -0.1152191, -0.7790698),
    (-0.5067279, -0.3154370, -0.8023256),
    (0.1518000, 0.5434815, -0.8255814),
    (0.2390716, -0.4715084, -0.8488372),
]


def direction_compute(norm: List[float]) -> Tuple[float, float, float, int]:
    """Exact port of direction_compute() from direction.c"""
    sum_w = 0.0
    wx = wy = wz = 0.0
    active_count = 0

    for i in range(SENSOR_COUNT):
        sx, sy, sz = SENSOR_POSITIONS[i]
        if sz < Z_EXCLUDE_THRESHOLD:
            continue

        w = norm[i]
        if w < 0.001:
            continue

        wx += w * sx
        wy += w * sy
        wz += w * sz
        sum_w += w
        active_count += 1

    if active_count < 3 or sum_w < 0.001:
        return (0.0, 0.0, 1.0, -1)

    cx = wx / sum_w
    cy = wy / sum_w
    cz = wz / sum_w

    length = math.sqrt(cx * cx + cy * cy + cz * cz)
    if length < 0.001:
        return (0.0, 0.0, 1.0, -1)

    return (cx / length, cy / length, cz / length, 0)


def vec_len(x, y, z):
    return math.sqrt(x * x + y * y + z * z)


def vec_dot(x1, y1, z1, x2, y2, z2):
    return x1 * x2 + y1 * y2 + z1 * z2


def angle_between(x1, y1, z1, x2, y2, z2):
    """Angle in degrees between two unit vectors"""
    d = max(-1.0, min(1.0, vec_dot(x1, y1, z1, x2, y2, z2)))
    return math.degrees(math.acos(d))


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


def about(a, b, tol=1e-5):
    return abs(a - b) < tol


def test_all_dark():
    """All sensors zero -> should return error with (0,0,1)"""
    readings = [0.0] * SENSOR_COUNT
    x, y, z, ret = direction_compute(readings)
    check("all-dark returns error", ret == -1)
    check("all-dark points up", about(x, 0.0) and about(y, 0.0) and about(z, 1.0))


def test_very_dim():
    """All sensors below threshold (1e-5) -> should return error"""
    readings = [1e-5] * SENSOR_COUNT
    x, y, z, ret = direction_compute(readings)
    check("dim returns error", ret == -1)


def test_single_top():
    """Only top sensor bright -> returns error (need >=3 active sensors)"""
    readings = [0.0] * SENSOR_COUNT
    readings[0] = 1.0
    x, y, z, ret = direction_compute(readings)
    check("single sensor returns error", ret == -1)


def test_single_equator():
    """A single sensor at equator -> returns error (need >=3 active)"""
    readings = [0.0] * SENSOR_COUNT
    readings[21] = 1.0
    x, y, z, ret = direction_compute(readings)
    check("single-equator returns error", ret == -1)


def test_two_symmetric():
    """Two equal sensors -> returns error (need >=3 active sensors)"""
    readings = [0.0] * SENSOR_COUNT
    readings[8] = 0.5
    readings[13] = 0.5
    x, y, z, ret = direction_compute(readings)
    check("two-symmetric returns error", ret == -1)


def test_only_bottom_sensors():
    """Lowest sensors (z near -0.85) are still above exclusion threshold"""
    # All 80 Fibonacci sphere sensors have z >= -0.8488 > -0.866
    # So the exclusion logic is a safety net that doesn't trigger normally
    lowest_z = min(p[2] for p in SENSOR_POSITIONS)
    check("lowest sensor above threshold", lowest_z >= Z_EXCLUDE_THRESHOLD,
          f"lowest z={lowest_z:.4f}, threshold={Z_EXCLUDE_THRESHOLD:.4f}")
    check("no sensors excluded by default", lowest_z > Z_EXCLUDE_THRESHOLD)


def test_uniform_illumination():
    """All sensors reading 1.0 -> direction should be near (0,0,1)"""
    readings = [1.0] * SENSOR_COUNT
    x, y, z, ret = direction_compute(readings)
    check("uniform success", ret == 0)
    check("uniform unit", about(vec_len(x, y, z), 1.0, 1e-4))
    ang = angle_between(x, y, z, 0, 0, 1)
    check("uniform points up", ang < 15.0, f"got {ang:.2f}deg")


def test_unit_vector_output():
    """Verify that direction_compute always returns unit vectors"""
    for seed in range(20):
        import random
        random.seed(seed)
        readings = [random.random() * 2.0 for _ in range(SENSOR_COUNT)]
        x, y, z, ret = direction_compute(readings)
        if ret == 0:
            vlen = vec_len(x, y, z)
            check(f"unit-vec seed={seed}", about(vlen, 1.0, 1e-4), f"got length {vlen:.6f}")


def test_weighted_response():
    """Brighter sensor has proportionally more weight"""
    readings = [0.0] * SENSOR_COUNT
    readings[0] = 0.3  # top sensor, z=0.9884
    readings[8] = 0.7  # z=0.8023
    readings[21] = 0.01  # z=0.5000 (needed for >=3 sensors)
    x, y, z, ret = direction_compute(readings)
    check("weighted success", ret == 0)
    check("weighted unit", about(vec_len(x, y, z), 1.0, 1e-4))
    ang0 = angle_between(x, y, z, *SENSOR_POSITIONS[0])
    ang8 = angle_between(x, y, z, *SENSOR_POSITIONS[8])
    check("weighted closer to brighter", ang8 < ang0, f"ang0={ang0:.1f} ang8={ang8:.1f}")


def test_sum_weight_minimum():
    """Exactly at the sum_w minimum (0.001) with 3 sensors -> works"""
    # 3 sensors at 0.00034 each = 0.00102 > 0.001, all >= 0.001 threshold... 
    # Actually threshold is per-sensor w < 0.001, so we need w >= 0.001 per sensor
    readings = [0.0] * SENSOR_COUNT
    readings[0] = 0.001  # exactly at threshold
    readings[1] = 0.001
    readings[2] = 0.001
    x, y, z, ret = direction_compute(readings)
    # sum_w = 0.003 > 0.001, active=3 >= 3
    check("minimum valid sum succeeds", ret == 0)


def test_exactly_three_sensors():
    """Edge case: exactly 3 active sensors"""
    readings = [0.0] * SENSOR_COUNT
    readings[0] = 1.0
    readings[21] = 1.0
    readings[42] = 1.0
    x, y, z, ret = direction_compute(readings)
    check("exactly-3 succeeds", ret == 0)
    check("exactly-3 unit", about(vec_len(x, y, z), 1.0, 1e-4))


def test_two_sensors_fails():
    """Only 2 active sensors -> should fail"""
    readings = [0.0] * SENSOR_COUNT
    readings[0] = 1.0
    readings[21] = 1.0
    x, y, z, ret = direction_compute(readings)
    check("only-2 returns error", ret == -1)


# ---- Run ----

if __name__ == "__main__":
    print("=== direction.c algorithm tests ===\n")

    test_all_dark()
    test_very_dim()
    test_single_top()
    test_single_equator()
    test_two_symmetric()
    test_only_bottom_sensors()
    test_uniform_illumination()
    test_unit_vector_output()
    test_weighted_response()
    test_sum_weight_minimum()
    test_exactly_three_sensors()
    test_two_sensors_fails()

    print(f"\n--- Results: {passed} passed, {failed} failed ---")
    sys.exit(0 if failed == 0 else 1)
