"""
Rigorous validation of auto-calibration accuracy.
Tests: with randomly scrambled wiring, apply sort-and-map calibration,
then measure the DIRECTION ERROR (what the solar array actually uses).
"""
import math
import random
import sys

# Sensor positions and algorithm (same as firmware)
SENSOR_POSITIONS = [
    (0.1520546, 0.0000000, 0.9883721), (-0.1930591, -0.1768580, 0.9651163),
    (0.0293754, 0.3347177, 0.9418605), (0.2404415, -0.3136134, 0.9186047),
    (-0.4385574, 0.0775746, 0.8953488), (0.4128833, 0.2626426, 0.8720930),
    (-0.1372409, -0.5105294, 0.8488372), (-0.2600820, 0.5007721, 0.8255814),
    (0.5606683, -0.2047552, 0.8023256), (-0.5795066, -0.2392119, 0.7790698),
    (0.2775286, 0.5930625, 0.7558140), (0.2037239, -0.6495038, 0.7325581),
    (-0.6098912, 0.3534444, 0.7093023), (0.7105880, 0.1562208, 0.6860465),
    (-0.4306596, -0.6125690, 0.6627907), (-0.0987941, 0.7623876, 0.6395349),
    (0.6021824, -0.5075199, 0.6162791), (-0.8044977, -0.0332685, 0.5930233),
    (0.5825202, 0.5796855, 0.5697674), (-0.0386831, -0.8365576, 0.5465116),
    (-0.5459967, 0.6542866, 0.5232558), (0.8582913, -0.1154819, 0.5000000),
    (-0.7215690, -0.5020489, 0.4767442), (0.1956154, 0.8695303, 0.4534884),
    (0.4488142, -0.7832406, 0.4302326), (-0.8702263, 0.2776259, 0.4069767),
    (0.8382989, 0.3873154, 0.3837209), (-0.3601072, -0.8604578, 0.3604651),
    (-0.3186290, 0.8858699, 0.3372093), (0.8404328, -0.4417079, 0.3139535),
    (-0.9252114, -0.2438827, 0.2906977), (0.5211372, 0.8104881, 0.2674419),
    (0.1642489, -0.9557173, 0.2441860), (-0.7710866, 0.5971728, 0.2209302),
    (0.9769208, 0.0809359, 0.1976744), (-0.6686701, -0.7228128, 0.1744186),
    (0.0048222, 0.9884971, 0.1511628), (0.6663664, -0.7345717, 0.1279070),
    (-0.9902651, 0.0917775, 0.1046512), (0.7939164, 0.6025542, 0.0813953),
    (-0.1786808, -0.9821878, 0.0581395), (-0.5322814, 0.8458484, 0.0348837),
    (0.9643720, -0.2642944, 0.0116279), (-0.8896262, -0.4565415, -0.0116279),
    (0.3474053, 0.9370660, -0.0348837), (0.3764049, -0.9246292, -0.0581395),
    (-0.9006574, 0.4268384, -0.0813953), (0.9503655, 0.2930078, -0.1046512),
    (-0.5014693, -0.8556684, -0.1279070), (-0.2075398, 0.9664766, -0.1511628),
    (0.8027506, -0.5702364, -0.1744186), (-0.9727424, -0.1212310, -0.1976744),
    (0.6321530, 0.7426792, -0.2209302), (0.0353400, -0.9690842, -0.2441860),
    (-0.6763458, 0.6863172, -0.2674419), (0.9555667, -0.0488588, -0.2906977),
    (-0.7319221, -0.6047505, -0.3139535), (0.1300869, 0.9323987, -0.3372093),
    (0.5289947, -0.7682640, -0.3604651), (-0.8999329, 0.2070725, -0.3837209),
    (0.7947481, 0.4502725, -0.4069767), (-0.2785597, -0.8586643, -0.4302326),
    (-0.3698641, 0.8108939, -0.4534884), (0.8092276, -0.3433157, -0.4767442),
    (-0.8163358, -0.2891294, -0.5000000), (0.4001335, 0.7523939, -0.5232558),
    (0.2095053, -0.8108222, -0.5465116), (-0.6890667, 0.4478305, -0.5697674),
    (0.7942074, 0.1325064, -0.5930233), (-0.4852373, -0.6202780, -0.6162791),
    (-0.0597347, 0.7664378, -0.6395349), (0.5471841, -0.5111732, -0.6627907),
    (-0.7275231, 0.0070984, -0.6860465), (0.5243954, 0.4710623, -0.7093023),
    (-0.0661241, -0.6774852, -0.7325581), (-0.3933087, 0.5235012, -0.7558140),
    (0.6162588, -0.1152191, -0.7790698), (-0.5067279, -0.3154370, -0.8023256),
    (0.1518000, 0.5434815, -0.8255814), (0.2390716, -0.4715084, -0.8488372),
]

S = len(SENSOR_POSITIONS)
Z_MIN = -0.8660254

NOISE_MISMATCH = 0.05
NOISE_ADC = 0.005


def vdot(a, b):
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]


def vlen(v):
    return math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)


def normalize(v):
    l = vlen(v)
    return (v[0]/l, v[1]/l, v[2]/l) if l > 1e-10 else (0, 0, 1)


def angle(a, b):
    d = max(-1, min(1, vdot(normalize(a), normalize(b))))
    return math.degrees(math.acos(d))


def generate_raw_readings(sun, noise=True):
    """For each POSITION, compute expected raw ADC reading.
    ALS-PT19 inverts: bright → low ADC. Returns raw readings (0=very bright, 1=dark)."""
    s = normalize(sun)
    readings = []
    for pos in SENSOR_POSITIONS:
        r = max(0.0, vdot(pos, s))
        if noise:
            r *= 1.0 + random.gauss(0, NOISE_MISMATCH)
            r += random.gauss(0, NOISE_ADC)
            r = max(0.0, min(1.0, r))
        readings.append(1.0 - r)  # invert: bright → low raw
    return readings


def scramble_readings(pos_readings, perm):
    """Apply permutation: physical[phys] = pos_readings[perm_inv(phys)]"""
    inv = [0]*S
    for pos, phys in enumerate(perm):
        inv[phys] = pos
    raw = [0.0]*S
    for phys in range(S):
        raw[phys] = pos_readings[inv[phys]]
    return raw, inv  # raw[phys], inv[phys]=true_position


def auto_calibrate(raw_readings, sun):
    """Sort-and-map auto-calibration. Returns phys->pos mapping.
    Matches calib_run_sun_auto(): ascending sort, -raw for channel inversion."""
    s = normalize(sun)
    predicted = [(max(0.0, vdot(SENSOR_POSITIONS[i], s)), i) for i in range(S)]
    predicted.sort()  # ascending: dimmest first
    # Channels: sort ascending by -raw (dimmest first, since bright→low raw, -low = high)
    raw_sorted = sorted(enumerate(raw_readings), key=lambda x: -x[1])
    mapping = [0xFF]*S
    for rank, (phys, _) in enumerate(raw_sorted):
        if rank < len(predicted):
            mapping[phys] = predicted[rank][1]
    return mapping


def compute_direction(readings_by_position, sun):
    """Weighted centroid algorithm (ported from direction.c)."""
    s = normalize(sun)
    wx = wy = wz = 0.0
    sw = 0.0
    n = 0
    for i in range(S):
        if SENSOR_POSITIONS[i][2] < Z_MIN:
            continue
        w = readings_by_position[i]
        if w < 0.001:
            continue
        wx += w * SENSOR_POSITIONS[i][0]
        wy += w * SENSOR_POSITIONS[i][1]
        wz += w * SENSOR_POSITIONS[i][2]
        sw += w
        n += 1
    if n < 3 or sw < 0.001:
        return None
    cx, cy, cz = wx/sw, wy/sw, wz/sw
    return normalize((cx, cy, cz))


def apply_mapping(raw, mapping):
    """Convert raw[phys] to output[pos] using mapping[phys]=pos.
    Inverts 1.0 - raw/max to match sensor_scan_all() normalization."""
    out = [0.0]*S
    max_r = max(raw) if max(raw) > 0 else 1.0
    for phys in range(S):
        pos = mapping[phys]
        if pos < S:
            out[pos] = 1.0 - raw[phys] / max_r
    return out


# ========== MAIN TEST ==========

print("=== Auto-Calibration: Direction Error Test ===")
print("For each sun position: scramble wiring → auto-cal → measure direction error\n")

sun_positions = [
    (0, 0, 1, "zenith"),
    (0.3, 0.2, 0.93, "high"),
    (0.5, 0.3, 0.81, "mid-high"),
    (0.7, 0.4, 0.59, "mid"),
    (0.85, 0.3, 0.43, "mid-low"),
    (0.95, 0.2, 0.24, "low"),
    (1, 0, 0.05, "horizon"),
]

print(f"{'Sun':>10s} | {'No Calib':>10s} | {'Coarse Cal':>10s} | {'Perfect':>10s} | {'Calib ok?':>10s}")
print("-" * 62)

for sx, sy, sz, label in sun_positions:
    sun = (sx, sy, sz)
    err_nocalib = []
    err_coarse = []
    err_perfect = []

    for trial in range(200):
        random.seed(trial * 100 + 1)

        # Generate true sensor readings at each position
        pos_readings = generate_raw_readings(sun, noise=True)

        # Random wiring scramble
        perm = list(range(S))
        random.shuffle(perm)

        # Get physical channel readings and true mapping
        raw, true_phys_to_pos = scramble_readings(pos_readings, perm)

        # --- Test 1: No calibration (identity mapping) ---
        identity_out = [raw[i] / max(raw) if max(raw) > 0 else 0 for i in range(S)]
        d = compute_direction(identity_out, sun)
        if d:
            err_nocalib.append(angle(d, sun))

        # --- Test 2: Coarse auto-calibration ---
        calib_map = auto_calibrate(raw, sun)
        coarse_out = apply_mapping(raw, calib_map)
        d = compute_direction(coarse_out, sun)
        if d:
            err_coarse.append(angle(d, sun))

        # --- Test 3: Perfect mapping (ground truth baseline) ---
        perfect_out = apply_mapping(raw, true_phys_to_pos)
        d = compute_direction(perfect_out, sun)
        if d:
            err_perfect.append(angle(d, sun))

    if err_nocalib and err_coarse and err_perfect:
        a_n = sum(err_nocalib)/len(err_nocalib)
        a_c = sum(err_coarse)/len(err_coarse)
        a_p = sum(err_perfect)/len(err_perfect)
        delta = a_c - a_p
        status = "OK" if delta < 1.0 else ("WARN" if delta < 3.0 else "FAIL")
        print(f"{label:>10s} | {a_n:9.2f}° | {a_c:9.2f}° | {a_p:9.2f}° | {delta:+6.2f}° {status}")

print("-" * 62)
print("No Calib = identity mapping (worst case)")
print("Coarse Cal = sort-and-map auto-calibration")
print("Perfect = ground truth wiring (best possible)")
print("Calib ok? = additional error from calibration vs perfect")
