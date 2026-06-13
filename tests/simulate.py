#!/usr/bin/env python3
"""
Solar Ball Desktop Simulator
=============================
Simulates the full sensor→direction pipeline without hardware.

For a given sun direction, computes expected sensor readings using
Lambert's cosine law (I = I_max * max(0, dot(surface_normal, light_dir))),
then runs the weighted centroid algorithm and reports angular error.

Usage:
    python simulate.py                          # random sun positions
    python simulate.py --sun 0.5 0.2 0.85       # specific sun direction
    python simulate.py --scan                   # sweep across the sky
"""

import math
import sys
import random
from typing import List, Tuple

# ---- Sensor positions (from sensor_positions.h) ----
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

SENSOR_COUNT = len(SENSOR_POSITIONS)
Z_EXCLUDE_THRESHOLD = -0.8660254  # cos(150deg)

# ---- Noise simulation ----
ADC_NOISE_STDDEV = 0.005    # ADC noise (% of full scale)
SENSOR_MISMATCH = 0.05      # sensor-to-sensor sensitivity variation (%)


def vec_len(x, y, z):
    return math.sqrt(x * x + y * y + z * z)


def vec_dot(x1, y1, z1, x2, y2, z2):
    return x1 * x2 + y1 * y2 + z1 * z2


def vec_normalize(x, y, z):
    length = vec_len(x, y, z)
    if length < 1e-10:
        return (0.0, 0.0, 1.0)
    return (x / length, y / length, z / length)


def angle_between(x1, y1, z1, x2, y2, z2):
    d = max(-1.0, min(1.0, vec_dot(x1, y1, z1, x2, y2, z2)))
    return math.degrees(math.acos(d))


def compute_sensor_readings(sun_x, sun_y, sun_z, noise: bool = True) -> List[float]:
    """
    Compute expected sensor readings for a given sun direction.
    Uses Lambert's cosine law: reading = max(0, dot(sensor_normal, sun_dir))
    with simulated noise and sensor mismatch.
    """
    sun_norm = vec_normalize(sun_x, sun_y, sun_z)
    readings = []

    for i, (sx, sy, sz) in enumerate(SENSOR_POSITIONS):
        cos_angle = vec_dot(sx, sy, sz, *sun_norm)
        # ALS-PT19 has a roughly cosine response
        intensity = max(0.0, cos_angle)

        if noise:
            # Sensor-to-sensor sensitivity variation
            sensitivity = 1.0 + random.gauss(0.0, SENSOR_MISMATCH)
            # ADC quantization + thermal noise
            adc_noise = random.gauss(0.0, ADC_NOISE_STDDEV)
            intensity = max(0.0, min(1.0, intensity * sensitivity + adc_noise))

        readings.append(intensity)

    return readings


def direction_compute(norm: List[float]) -> Tuple[float, float, float, int]:
    """Port of direction_compute() from direction.c"""
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


def run_simulation(sun_x, sun_y, sun_z, label="", noise=True):
    """Run one simulation and report results."""
    sun_norm = vec_normalize(sun_x, sun_y, sun_z)
    readings = compute_sensor_readings(*sun_norm, noise=noise)
    dx, dy, dz, ret = direction_compute(readings)

    if ret != 0:
        print(f"  {label}: FAILED (algorithm returned error)")
        return None

    error_deg = angle_between(dx, dy, dz, *sun_norm)
    return error_deg


def scan_sky(noise=True):
    """Sweep sun positions across the sky and report angular errors."""
    print(f"\n{'Elev':>6s} {'Azim':>6s} {'Error':>8s}  Status")
    print("-" * 40)

    errors = []
    for elev in [90, 75, 60, 45, 30, 15, 5]:
        for azim in [0, 45, 90, 135, 180, 225, 270, 315]:
            el_rad = math.radians(elev)
            az_rad = math.radians(azim)
            sx = math.cos(el_rad) * math.cos(az_rad)
            sy = math.cos(el_rad) * math.sin(az_rad)
            sz = math.sin(el_rad)

            error = run_simulation(sx, sy, sz, noise=noise)
            if error is not None:
                errors.append(error)
                status = "OK" if error < 10 else ("WARN" if error < 20 else "FAIL")
                print(f"  {elev:5.0f}° {azim:5.0f}° {error:7.2f}°  {status}")

    if errors:
        avg = sum(errors) / len(errors)
        worst = max(errors)
        best = min(errors)
        print("-" * 40)
        print(f"  Summary: avg={avg:.2f}°  best={best:.2f}°  worst={worst:.2f}°")
        return avg, best, worst
    return None


def random_suns(count=20, noise=True):
    """Test random sun positions."""
    print(f"\nRandom sun positions ({count} samples):")
    errors = []
    for i in range(count):
        # Random direction on upper hemisphere
        theta = math.acos(random.random())  # 0 to pi
        phi = random.uniform(0, 2 * math.pi)
        sx = math.sin(theta) * math.cos(phi)
        sy = math.sin(theta) * math.sin(phi)
        sz = abs(math.cos(theta))  # ensure upper hemisphere

        error = run_simulation(sx, sy, sz, noise=noise)
        if error is not None:
            errors.append(error)

    if errors:
        avg = sum(errors) / len(errors)
        print(f"  Avg error: {avg:.2f}° (min={min(errors):.2f}°, max={max(errors):.2f}°)")
        return avg
    return None


def auto_calibrate_from_sun(sun_x, sun_y, sun_z, noise=True, scrambled=True):
    """
    Simulate the auto-calibration algorithm:
    1. If scrambled, create a random permutation of the channel->position mapping
    2. Generate sensor readings for given sun direction
    3. Run sort-and-map algorithm to recover the mapping
    4. Report accuracy (how many channels correctly mapped)
    """
    sun_norm = vec_normalize(sun_x, sun_y, sun_z)

    # Create scrambled mapping (position -> physical channel)
    if scrambled:
        import random
        pos_to_phys = list(range(SENSOR_COUNT))
        random.shuffle(pos_to_phys)
    else:
        pos_to_phys = list(range(SENSOR_COUNT))

    # Invert: physical -> position (this is what we need to recover)
    phys_to_pos = [0] * SENSOR_COUNT
    for pos, phys in enumerate(pos_to_phys):
        phys_to_pos[phys] = pos

    # Generate readings (with noise) for each POSITION, then remap to physical channels
    readings_per_position = []
    for i, (sx, sy, sz) in enumerate(SENSOR_POSITIONS):
        cos_angle = vec_dot(sx, sy, sz, *sun_norm)
        intensity = max(0.0, cos_angle)
        if noise:
            sensitivity = 1.0 + random.gauss(0.0, SENSOR_MISMATCH)
            adc_noise = random.gauss(0.0, ADC_NOISE_STDDEV)
            intensity = max(0.0, min(1.0, intensity * sensitivity + adc_noise))
        readings_per_position.append(intensity)

    # Map to physical channels
    raw_per_physical = [0.0] * SENSOR_COUNT
    for pos, phys in enumerate(pos_to_phys):
        raw_per_physical[phys] = readings_per_position[pos]

    if not scrambled:
        return None  # nothing to calibrate

    # === AUTO-CALIBRATION ALGORITHM ===
    # 1. Compute predicted reading for each position given sun direction
    predicted = []
    for i, (sx, sy, sz) in enumerate(SENSOR_POSITIONS):
        pred = max(0.0, vec_dot(sx, sy, sz, *sun_norm))
        predicted.append((pred, i))

    # 2. Sort positions by predicted reading (descending)
    predicted.sort(key=lambda x: -x[0])

    # 3. Sort physical channels by raw reading (descending)
    raw_sorted = sorted(enumerate(raw_per_physical), key=lambda x: -x[1])

    # 4. Greedy mapping: highest-reading physical -> highest-predicted position
    recovered_phys_to_pos = [0xFF] * SENSOR_COUNT
    for rank, (phys_ch, _) in enumerate(raw_sorted):
        if rank < len(predicted):
            recovered_phys_to_pos[phys_ch] = predicted[rank][1]

    # 5. Compare against ground truth
    correct = 0
    wrong = []
    for phys in range(SENSOR_COUNT):
        if recovered_phys_to_pos[phys] == phys_to_pos[phys]:
            correct += 1
        else:
            wrong.append((phys, phys_to_pos[phys], recovered_phys_to_pos[phys]))

    return correct, wrong, predicted, raw_sorted


def test_auto_cal():
    """Run auto-calibration simulation with various sun positions."""
    print("\n=== Auto Sun-Calibration Simulation ===")
    print("Testing: coarse sort-and-map → direction error with noisy mapping\n")

    # For each test case:
    # 1. Create random channel→position scramble
    # 2. Do coarse sort-and-map calibration under sun
    # 3. Measure direction error using the COARSE mapping (not perfect ground truth)
    # 4. Compare with direction error using PERFECT mapping

    import random

    for elev, azim in [
        (90, 0), (60, 45), (45, 90), (30, 135), (15, 180),
    ]:
        el_r, az_r = math.radians(elev), math.radians(azim)
        sun_x = math.cos(el_r) * math.cos(az_r)
        sun_y = math.cos(el_r) * math.sin(az_r)
        sun_z = math.sin(el_r)

        # Create scrambles and test multiple random seeds
        err_direct = []  # error using coarse mapping
        err_perfect = [] # error using ground truth mapping (baseline)

        for seed in range(10):
            random.seed(seed)
            sun_norm = vec_normalize(sun_x, sun_y, sun_z)

            # Scramble: position -> physical
            pos_to_phys = list(range(SENSOR_COUNT))
            random.shuffle(pos_to_phys)

            # Ground truth: physical -> position
            phys_to_pos = [0] * SENSOR_COUNT
            for pos, phys in enumerate(pos_to_phys):
                phys_to_pos[phys] = pos

            # Generate readings at each POSITION
            readings_per_pos = []
            for i, (sx, sy, sz) in enumerate(SENSOR_POSITIONS):
                cos_angle = vec_dot(sx, sy, sz, *sun_norm)
                intensity = max(0.0, cos_angle)
                sensitivity = 1.0 + random.gauss(0.0, SENSOR_MISMATCH)
                adc_noise = random.gauss(0.0, ADC_NOISE_STDDEV)
                intensity = max(0.0, min(1.0, intensity * sensitivity + adc_noise))
                readings_per_pos.append(intensity)

            # Physical channel readings
            raw = [0.0] * SENSOR_COUNT
            for pos, phys in enumerate(pos_to_phys):
                raw[phys] = readings_per_pos[pos]

            # === Coarse calibration: sort-and-map ===
            predicted = [(max(0.0, vec_dot(*SENSOR_POSITIONS[i], *sun_norm)), i) for i in range(SENSOR_COUNT)]
            predicted.sort(key=lambda x: -x[0])
            raw_sorted = sorted(enumerate(raw), key=lambda x: -x[1])

            coarse_phys_to_pos = [0xFF] * SENSOR_COUNT
            for rank, (phys, _) in enumerate(raw_sorted):
                if rank < len(predicted):
                    coarse_phys_to_pos[phys] = predicted[rank][1]

            # === Test direction accuracy with both mappings ===
            # Apply coarse mapping to get position-indexed readings
            coarse_out = [0.0] * SENSOR_COUNT
            for phys in range(SENSOR_COUNT):
                pos = coarse_phys_to_pos[phys]
                if pos < SENSOR_COUNT:
                    coarse_out[pos] = raw[phys] / (max(raw) if max(raw) > 0 else 1)

            # Apply perfect mapping
            perfect_out = [0.0] * SENSOR_COUNT
            for phys in range(SENSOR_COUNT):
                pos = phys_to_pos[phys]
                if pos < SENSOR_COUNT:
                    perfect_out[pos] = raw[phys] / (max(raw) if max(raw) > 0 else 1)

            # Compute direction with each mapping
            dx_c, dy_c, dz_c, ret_c = direction_compute(coarse_out)
            dx_p, dy_p, dz_p, ret_p = direction_compute(perfect_out)

            if ret_c == 0:
                err_direct.append(angle_between(dx_c, dy_c, dz_c, *sun_norm))
            if ret_p == 0:
                err_perfect.append(angle_between(dx_p, dy_p, dz_p, *sun_norm))

        if err_direct and err_perfect:
            avg_c = sum(err_direct) / len(err_direct)
            avg_p = sum(err_perfect) / len(err_perfect)
            delta = avg_c - avg_p
            print(f"  Sun elev={elev:2.0f}° az={azim:3.0f}° → "
                  f"coarse={avg_c:5.2f}°  perfect={avg_p:5.2f}°  "
                  f"(+{delta:+.2f}° penalty)")


def main():
    print("=== Solar Ball Desktop Simulator ===\n")

    args = sys.argv[1:]

    if "--sun" in args:
        idx = args.index("--sun")
        if idx + 3 < len(args):
            sx, sy, sz = float(args[idx + 1]), float(args[idx + 2]), float(args[idx + 3])
            error = run_simulation(sx, sy, sz, label="custom", noise=True)
            if error is not None:
                print(f"Angular error: {error:.2f}°")
        else:
            print("Usage: --sun <x> <y> <z>")
    elif "--autocal" in args:
        test_auto_cal()
    elif "--scan" in args:
        scan_sky(noise=True)
    elif "--noiseless" in args:
        # Noiseless mode: should give near-zero errors
        print("Noiseless mode (ideal sensors):\n")

        # Test a few key positions
        tests = [
            (0, 0, 1, "zenith"),
            (1, 0, 0, "horizon-east"),
            (0, 1, 0, "horizon-north"),
            (0.577, 0.577, 0.577, "45deg"),
            (0.5, 0.2, 0.842, "typical"),
        ]
        for sx, sy, sz, label in tests:
            error = run_simulation(sx, sy, sz, label=label, noise=False)
            print(f"  {label:>12s}: error = {error:.2f}°")

        scan_sky(noise=False)
    else:
        random_suns(count=50, noise=True)
        scan_sky(noise=True)


if __name__ == "__main__":
    main()
