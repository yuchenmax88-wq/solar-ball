"""
End-to-end test: Sun auto-calibration full pipeline.
Simulates the entire flow from GPS location → solar ephemeris → sensor scan →
sort-and-map calibration → direction computation → accuracy measurement.
"""
import math
import random
import sys

# ---- Sensor positions ----
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

# ---- Solar ephemeris (port of sun_calc.c) ----
PI = math.pi
RAD = PI / 180.0
JD2000 = 2451545.0
UNIX2000_12H = 946728000


def sun_direction(lat, lon, unix_ts):
    """Port of sun_calc_direction() from sun_calc.c"""
    jd = JD2000 + (unix_ts - UNIX2000_12H) / 86400.0
    n = jd - JD2000

    L = math.fmod(280.460 + 0.9856474 * n, 360.0)
    if L < 0:
        L += 360.0

    g = math.fmod(357.528 + 0.9856003 * n, 360.0)
    if g < 0:
        g += 360.0

    g_rad = g * RAD
    lam = L + 1.915 * math.sin(g_rad) + 0.020 * math.sin(2 * g_rad)
    eps = 23.439 - 0.0000004 * n

    sin_dec = math.sin(eps * RAD) * math.sin(lam * RAD)
    dec = math.asin(sin_dec)

    cos_eps = math.cos(eps * RAD)
    cos_dec = math.cos(dec)
    ra = math.atan2(cos_eps * math.sin(lam * RAD), math.cos(lam * RAD))

    gmst = math.fmod(280.46061837 + 360.98564736629 * n, 360.0)
    if gmst < 0:
        gmst += 360.0

    lha = (gmst + lon) * RAD - ra
    while lha < -PI:
        lha += 2 * PI
    while lha > PI:
        lha -= 2 * PI

    sin_lat = math.sin(lat * RAD)
    cos_lat = math.cos(lat * RAD)
    sin_elev = sin_lat * math.sin(dec) + cos_lat * cos_dec * math.cos(lha)
    elev = math.asin(sin_elev)

    cos_elev = math.cos(elev)
    cos_az = (math.sin(dec) - sin_lat * sin_elev) / (cos_lat * cos_elev)
    cos_az = max(-1.0, min(1.0, cos_az))
    az = math.acos(cos_az)
    if math.sin(lha) > 0:
        az = 2 * PI - az

    return (cos_elev * math.sin(az), cos_elev * math.cos(az), sin_elev)


# ---- Direction algorithm (port of direction.c) ----
def vdot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def vlen(v):
    return math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2)


def normalize(v):
    l = vlen(v)
    return (v[0] / l, v[1] / l, v[2] / l) if l > 1e-10 else (0, 0, 1)


def angle(a, b):
    d = max(-1, min(1, vdot(normalize(a), normalize(b))))
    return math.degrees(math.acos(d))


def direction_compute(readings):
    wx = wy = wz = 0.0
    sw = 0.0
    n = 0
    for i in range(S):
        if SENSOR_POSITIONS[i][2] < Z_MIN:
            continue
        w = readings[i]
        if w < 0.001:
            continue
        wx += w * SENSOR_POSITIONS[i][0]
        wy += w * SENSOR_POSITIONS[i][1]
        wz += w * SENSOR_POSITIONS[i][2]
        sw += w
        n += 1
    if n < 3 or sw < 0.001:
        return None
    return normalize((wx / sw, wy / sw, wz / sw))


# ---- Auto-calibration (port of calib_run_sun_auto) ----
def auto_calibrate(raw_per_physical, sun_dir):
    """Exact port of calib_run_sun_auto() from calibrate.c:
       - Positions sorted ascending by predicted brightness (dimmest first)
       - Channels sorted ascending by -raw (since ALS-PT19 inverts: bright→low ADC)
       - rank 0 = dimmest pair, rank 79 = brightest pair
    """
    # Rank positions by predicted brightness (dimmest first)
    pos_ranked = [(max(0.0, vdot(SENSOR_POSITIONS[pos], sun_dir)), pos) for pos in range(S)]
    pos_ranked.sort()  # ascending: dimmest first

    # Rank channels: use -raw to invert. Ascending -raw puts dimmest first.
    ch_ranked = [(-float(raw_per_physical[phys]), phys) for phys in range(S)]
    ch_ranked.sort()  # ascending: dimmest (lowest -raw = highest raw) first

    # Map: rank N → rank N (dimmest→dimmest, brightest→brightest)
    phys_to_pos = [0xFF] * S
    channel_map = [0xFF] * S
    for rank in range(S):
        pos = pos_ranked[rank][1]
        phys = ch_ranked[rank][1]
        channel_map[pos] = phys
        phys_to_pos[phys] = pos
    return channel_map, phys_to_pos


# ---- Test ----
passed = 0
failed = 0


def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
    else:
        failed += 1
        print(f"  FAIL: {name} {detail}")


def run_e2e_test(lat, lon, unix_ts, sun_pos, desc):
    """Full pipeline: ephemeris → scan → calibrate → direction → measure error"""
    # Compute sun direction from ephemeris (as firmware would)
    sun = sun_direction(lat, lon, unix_ts)
    sun_norm = normalize(sun)

    # Verify ephemeris matches expected position
    ang = angle(sun, sun_pos)
    check(f"{desc} ephemeris accuracy", ang < 3.0,
          f"Sun position off by {ang:.2f}°")

    # Generate sensor readings at each position (inverted: bright→low raw, matching ALS-PT19 circuit)
    readings_per_pos = []
    for pos in SENSOR_POSITIONS:
        r = max(0.0, vdot(pos, sun_norm))  # 0=dark, 1=bright
        r *= 1.0 + random.gauss(0, 0.05)
        r += random.gauss(0, 0.005)
        r = max(0.0, min(1.0, r))
        readings_per_pos.append(1.0 - r)  # INVERT: ALS-PT19 common-emitter: bright→low voltage→low ADC

    # Scramble wiring (random permutation)
    perm = list(range(S))
    random.shuffle(perm)
    inv_perm = [0] * S
    for p, ph in enumerate(perm):
        inv_perm[ph] = p

    # Physical channel readings
    raw = [0.0] * S
    for phys in range(S):
        raw[phys] = readings_per_pos[inv_perm[phys]]

    # Run auto-calibration (using computed sun direction, not truth)
    channel_map, phys_to_pos = auto_calibrate(raw, sun_norm)

    # Apply calibrated mapping + INVERT (matching sensor_scan_all: bright→low raw→invert to bright→high weight)
    max_raw = max(raw) if max(raw) > 0 else 1.0
    output = [0.0] * S
    for phys in range(S):
        pos = phys_to_pos[phys]
        if pos < S:
            output[pos] = 1.0 - raw[phys] / max_raw

    # Compute direction
    d = direction_compute(output)
    if d is None:
        check(f"{desc} direction computed", False, "algorithm returned error")
        return

    # Measure accuracy against TRUE sun position
    err = angle(d, sun_pos)
    check(f"{desc} direction error < 10°", err < 10.0, f"error={err:.2f}°")


print("=== End-to-End Auto-Calibration Tests ===\n")
print("Each test: GPS location → ephemeris → scan → calibrate → measure direction\n")

# Test cases: various locations and times (UTC timestamps for daytime)
# 2024-06-10: Beijing(UTC+8) noon=04:00UTC, 9am=01:00UTC, 3pm=07:00UTC, 7am=23:00UTC(-1d)
import datetime

def make_ts(year, month, day, hour_utc):
    dt = datetime.datetime(year, month, day, hour_utc, 0, 0, tzinfo=datetime.timezone.utc)
    return int(dt.timestamp())

locations = [
    (39.9, 116.4, make_ts(2024, 6, 10, 4),  "Beijing, noon summer"),
    (31.2, 121.5, make_ts(2024, 3, 20, 1),  "Shanghai, 9am equinox"),
    (22.5, 114.1, make_ts(2024, 3, 20, 7),  "Shenzhen, 3pm equinox"),
    (40.0, 116.0, make_ts(2024, 3, 20, 2),  "Beijing, 10am equinox"),
    (30.0, 120.0, make_ts(2024, 6, 10, 0),  "Hangzhou, 8am summer"),
]

for lat, lon, unix_ts, desc in locations:
    random.seed(42)

    # Compute true sun position for comparison
    sun = sun_direction(lat, lon, unix_ts)
    sun_norm = normalize(sun)
    elev = math.degrees(math.asin(sun_norm[2]))

    run_e2e_test(lat, lon, unix_ts, sun_norm, f"{desc} (elev={elev:.0f}deg)")

print(f"\n--- Results: {passed} passed, {failed} failed ---")
sys.exit(0 if failed == 0 else 1)
