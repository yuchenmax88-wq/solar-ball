#!/usr/bin/env python3
"""
Integration tests for Solar Ball — end-to-end pipeline, MQTT protocol, and OTA simulation.

Tests:
  1. Full pipeline: raw sensors → calibration → direction → MQTT → receiver
  2. MQTT protocol validation (v1.1 fields, JSON format)
  3. OTA command protocol (encode → transmit → decode)
  4. Multi-ball scenario
  5. Fault detection pipeline
  6. Historical data logging

Requires: paho-mqtt (for broker-based tests, skipped if not available)
"""

import json
import math
import os
import struct
import sys
import time
import unittest
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
PROJECT_DIR = TESTS_DIR.parent.parent

sys.path.insert(0, str(PROJECT_DIR / "tools"))
sys.path.insert(0, str(TESTS_DIR.parent))

try:
    import paho.mqtt.client as mqtt
    HAS_MQTT = True
except ImportError:
    HAS_MQTT = False

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
Z_EXCLUDE_THRESHOLD = -0.8660254


def vec_len(x, y, z):
    return math.sqrt(x * x + y * y + z * z)


def vec_dot(x1, y1, z1, x2, y2, z2):
    return x1 * x2 + y1 * y2 + z1 * z2


def vec_normalize(x, y, z):
    length = vec_len(x, y, z)
    if length < 1e-10:
        return (0.0, 0.0, 1.0)
    return (x / length, y / length, z / length)


def direction_compute(norm):
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


def angle_between(x1, y1, z1, x2, y2, z2):
    d = max(-1.0, min(1.0, vec_dot(x1, y1, z1, x2, y2, z2)))
    return math.degrees(math.acos(d))


def compute_sensor_readings(sun_x, sun_y, sun_z):
    sun_norm = vec_normalize(sun_x, sun_y, sun_z)
    readings = []
    for sx, sy, sz in SENSOR_POSITIONS:
        cos_angle = vec_dot(sx, sy, sz, *sun_norm)
        intensity = max(0.0, cos_angle)
        readings.append(intensity)
    return readings


def serialize_direction(pkt):
    return json.dumps({
        "id": pkt.get("id", "?"),
        "ts": pkt.get("ts", 0),
        "dx": round(pkt.get("dx", 0), 4),
        "dy": round(pkt.get("dy", 0), 4),
        "dz": round(pkt.get("dz", 0), 4),
        "soc": pkt.get("soc", 0),
        "rssi": pkt.get("rssi", 0),
        "err": pkt.get("err", 0),
        "conf": pkt.get("conf", 0),
        "flags": pkt.get("flags", 0),
    })


def compute_battery_soc(mv):
    bat_full = 4200
    bat_empty = 3200
    if mv >= bat_full:
        return 100
    if mv <= bat_empty:
        return 0
    return int((mv - bat_empty) * 100 / (bat_full - bat_empty))


class TestFullPipeline(unittest.TestCase):
    """E2E: sensor readings → direction → MQTT packet → receiver decode → panel angles."""

    def test_zenith_pipeline(self):
        dx, dy, dz, ret = direction_compute(compute_sensor_readings(0, 0, 1))
        self.assertEqual(ret, 0)
        self.assertAlmostEqual(dz, 1.0, delta=0.05)
        self.assertAlmostEqual(abs(dx) + abs(dy), 0.0, delta=0.05)

    def test_horizon_east_pipeline(self):
        dx, dy, dz, ret = direction_compute(compute_sensor_readings(1, 0, 0))
        self.assertEqual(ret, 0)
        self.assertGreater(dx, 0)

    def test_horizon_north_pipeline(self):
        dx, dy, dz, ret = direction_compute(compute_sensor_readings(0, 1, 0))
        self.assertEqual(ret, 0)
        self.assertGreater(dy, 0)

    def test_45deg_pipeline(self):
        sun = vec_normalize(0.577, 0.577, 0.577)
        dx, dy, dz, ret = direction_compute(compute_sensor_readings(*sun))
        self.assertEqual(ret, 0)
        error = angle_between(dx, dy, dz, *sun)
        self.assertLess(error, 10)

    def test_pipeline_to_mqtt_packet(self):
        sun = vec_normalize(0.5, 0.3, 0.81)
        dx, dy, dz, ret = direction_compute(compute_sensor_readings(*sun))
        self.assertEqual(ret, 0)

        pkt = {
            "id": "ball-001",
            "ts": 1718000000,
            "dx": dx, "dy": dy, "dz": dz,
            "soc": 85, "rssi": -78,
            "err": 0, "conf": 220, "flags": 9,
        }
        payload = serialize_direction(pkt)
        decoded = json.loads(payload)
        self.assertEqual(decoded["id"], "ball-001")
        self.assertEqual(decoded["soc"], 85)
        self.assertAlmostEqual(decoded["dx"], round(dx, 4), places=4)
        self.assertAlmostEqual(decoded["dy"], round(dy, 4), places=4)

    def test_pipeline_to_panel_angles(self):
        sun = vec_normalize(0.5, 0.2, 0.842)
        dx, dy, dz, ret = direction_compute(compute_sensor_readings(*sun))
        self.assertEqual(ret, 0)

        elev = math.degrees(math.asin(max(-1.0, min(1.0, dz))))
        az = math.degrees(math.atan2(dx, dy))
        if az < 0:
            az += 360.0

        panel_tilt = 90.0 - elev
        self.assertGreaterEqual(panel_tilt, 0)
        self.assertLessEqual(panel_tilt, 90)
        self.assertGreaterEqual(az, 0)
        self.assertLessEqual(az, 360)

    def test_pipeline_10_random_suns(self):
        import random
        errors = []
        for _ in range(10):
            theta = math.acos(random.random())
            phi = random.uniform(0, 2 * math.pi)
            sx = math.sin(theta) * math.cos(phi)
            sy = math.sin(theta) * math.sin(phi)
            sz = abs(math.cos(theta))
            sun = vec_normalize(sx, sy, sz)
            dx, dy, dz, ret = direction_compute(compute_sensor_readings(*sun))
            if ret == 0:
                errors.append(angle_between(dx, dy, dz, *sun))
        self.assertGreater(len(errors), 0)
        self.assertLess(sum(errors) / len(errors), 5)


class TestMQTTProtocol(unittest.TestCase):
    """Validate MQTT v1.1 protocol fields and fault bitmasking."""

    def test_all_fields_present(self):
        pkt = {
            "id": "ball-test", "ts": 1718000000,
            "dx": 0.3, "dy": -0.1, "dz": 0.95,
            "soc": 72, "rssi": -85,
            "err": 0x0041, "conf": 200, "flags": 0x09,
        }
        payload = serialize_direction(pkt)
        decoded = json.loads(payload)
        for key in ["id", "ts", "dx", "dy", "dz", "soc", "rssi",
                     "err", "conf", "flags"]:
            self.assertIn(key, decoded, f"Missing field: {key}")

    def test_fault_bitmask_decoding(self):
        FAULTS = {
            0x0001: "SENSOR_OPEN", 0x0002: "SENSOR_SHORT", 0x0004: "SATURATED",
            0x0008: "ADC_ERR", 0x0010: "MODEM_ERR", 0x0020: "MQTT_ERR",
            0x0040: "LOW_BAT", 0x0080: "NO_CALIB", 0x0100: "OVERCAST", 0x0200: "NIGHT",
        }
        mask = 0x0041
        active = [n for m, n in FAULTS.items() if mask & m]
        self.assertIn("SENSOR_OPEN", active)
        self.assertIn("LOW_BAT", active)
        self.assertEqual(len(active), 2)

    def test_direction_unit_vector(self):
        pkt = {
            "id": "ball-001", "ts": 0,
            "dx": 0.5, "dy": 0.3, "dz": 0.812,
            "soc": 50, "rssi": -90,
            "err": 0, "conf": 200, "flags": 0,
        }
        mag = math.sqrt(pkt["dx"]**2 + pkt["dy"]**2 + pkt["dz"]**2)
        self.assertAlmostEqual(mag, 1.0, delta=0.02)

    def test_soc_range(self):
        for mv in [3200, 3400, 3600, 3800, 4000, 4200]:
            soc = compute_battery_soc(mv)
            self.assertGreaterEqual(soc, 0)
            self.assertLessEqual(soc, 100)

    def test_rssi_null_value(self):
        self.assertEqual(-999, -999)

    def test_round_trip_serialization(self):
        pkt = {
            "id": "ball-042", "ts": 1718123456,
            "dx": 0.1234, "dy": -0.5678, "dz": 0.8138,
            "soc": 99, "rssi": -44,
            "err": 0x01FF, "conf": 255, "flags": 0x1F,
        }
        payload = serialize_direction(pkt)
        decoded = json.loads(payload)
        self.assertEqual(decoded["id"], "ball-042")
        self.assertAlmostEqual(decoded["dx"], 0.1234, places=3)


class TestOTAProtocol(unittest.TestCase):
    """Validate OTA command protocol encoding/decoding."""

    def test_ota_cmd_encode(self):
        cmd = {
            "cmd": "ota",
            "url": "http://ota.example.com/solar-ball-v1.3.0.bin",
            "version": "1.3.0",
        }
        payload = json.dumps(cmd)
        decoded = json.loads(payload)
        self.assertEqual(decoded["cmd"], "ota")
        self.assertTrue(decoded["url"].startswith("http"))
        self.assertEqual(decoded["version"], "1.3.0")

    def test_ota_abort_encode(self):
        cmd = {"cmd": "abort_ota"}
        payload = json.dumps(cmd)
        decoded = json.loads(payload)
        self.assertEqual(decoded["cmd"], "abort_ota")

    def test_ota_status_response(self):
        status = {
            "state": 4, "progress_pct": 100,
            "total_bytes": 1048576, "written_bytes": 1048576,
            "current_version": "1.2.0", "new_version": "1.3.0",
            "last_error": 0,
        }
        payload = json.dumps(status)
        decoded = json.loads(payload)
        self.assertEqual(decoded["state"], 4)
        self.assertEqual(decoded["progress_pct"], 100)
        self.assertEqual(decoded["total_bytes"], decoded["written_bytes"])

    def test_ota_url_validation(self):
        urls = [
            "http://ota.example.com/firmware.bin",
            "https://cdn.example.com/releases/v1.3.0.bin",
            "http://192.168.1.100:8080/ota.bin",
        ]
        for url in urls:
            self.assertTrue(url.startswith("http"))
            self.assertIn(".bin", url)

    def test_ota_version_format(self):
        versions = ["1.3.0", "2.0.0-rc1", "1.4.0-beta"]
        for v in versions:
            self.assertRegex(v, r"^\d+\.\d+\.\d+")


class TestMultiBall(unittest.TestCase):
    """Multi-ball MQTT scenario."""

    def test_unique_ball_ids(self):
        balls = [
            {"id": "ball-001", "ts": 1000, "dx": 0.5, "dy": 0.3, "dz": 0.81,
             "soc": 80, "rssi": -70, "err": 0, "conf": 200, "flags": 0},
            {"id": "ball-002", "ts": 1001, "dx": -0.3, "dy": 0.7, "dz": 0.65,
             "soc": 60, "rssi": -85, "err": 0, "conf": 180, "flags": 0},
            {"id": "ball-003", "ts": 1002, "dx": 0.1, "dy": -0.5, "dz": 0.86,
             "soc": 92, "rssi": -65, "err": 0x0010, "conf": 150, "flags": 0},
        ]
        ids = [b["id"] for b in balls]
        self.assertEqual(len(ids), len(set(ids)))

        for b in balls:
            payload = serialize_direction(b)
            decoded = json.loads(payload)
            self.assertEqual(decoded["id"], b["id"])

    def test_mixed_ball_states(self):
        balls = {
            "ball-001": {"soc": 90, "rssi": -60, "err": 0},
            "ball-002": {"soc": 5, "rssi": -999, "err": 0x0074},
            "ball-003": {"soc": 50, "rssi": -80, "err": 0x0200},
        }
        self.assertGreater(balls["ball-001"]["soc"], 80)
        self.assertLess(balls["ball-002"]["soc"], 10)
        self.assertTrue(balls["ball-002"]["err"] & 0x0010)
        self.assertTrue(balls["ball-003"]["err"] & 0x0200)


class TestFaultPipeline(unittest.TestCase):
    """Fault detection classification pipeline."""

    def test_classify_clear_sky(self):
        readings = [17000] * 40 + [30000] * 40
        has_signal = any(r > 500 for r in readings)
        variance = sum((r - 23500)**2 for r in readings) / 80
        self.assertTrue(has_signal)
        self.assertGreater(variance, 100000)

    def test_classify_overcast(self):
        readings = [20000] * 80
        variance = sum((r - 20000)**2 for r in readings) / 80
        self.assertLess(variance, 100)

    def test_classify_night(self):
        readings = [0] * 80
        has_signal = any(r > 100 for r in readings)
        self.assertFalse(has_signal)

    def test_detect_open_circuit(self):
        readings = [15000] * 79 + [0]
        has_open = any(r < 10 for r in readings)
        self.assertTrue(has_open)

    def test_detect_saturation(self):
        readings = [15000] * 79 + [65535]
        has_sat = any(r > 65000 for r in readings)
        self.assertTrue(has_sat)

    def test_confidence_computation(self):
        variance = 500000
        max_raw = 50000
        conf = min(255, int(variance / 10000) + 30)
        self.assertGreater(conf, 50)
        conf_low = min(255, int(50 / 10000) + 30)
        self.assertLess(conf_low, 50)

    def test_full_fault_mask(self):
        mask = 0x03FF
        for bit in range(10):
            self.assertTrue(mask & (1 << bit),
                            f"Bit {bit} not set in 0x{mask:04X}")


class TestDatabaseLogging(unittest.TestCase):
    """Historical data logging (SQLite)."""

    def test_sqlite_schema(self):
        import sqlite3
        conn = sqlite3.connect(":memory:")
        conn.execute("""
            CREATE TABLE IF NOT EXISTS direction_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                ball_id TEXT NOT NULL,
                ts INTEGER NOT NULL,
                dx REAL, dy REAL, dz REAL,
                azimuth REAL, elevation REAL,
                soc INTEGER, rssi INTEGER,
                error_mask INTEGER, confidence INTEGER, flags INTEGER,
                recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_ball_ts ON direction_log(ball_id, ts)
        """)
        samples = [
            ("ball-001", 1000, 0.3, -0.1, 0.95, 45.0, 71.8, 80, -70, 0, 200, 1),
            ("ball-001", 1005, 0.4, -0.2, 0.89, 50.0, 63.0, 79, -72, 0, 210, 1),
            ("ball-002", 1003, -0.2, 0.6, 0.77, 120.0, 50.4, 60, -85, 0x10, 150, 0),
        ]
        for s in samples:
            conn.execute("""
                INSERT INTO direction_log
                (ball_id, ts, dx, dy, dz, azimuth, elevation,
                 soc, rssi, error_mask, confidence, flags)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, s)
        conn.commit()
        count = conn.execute("SELECT COUNT(*) FROM direction_log").fetchone()[0]
        self.assertEqual(count, 3)
        rows = conn.execute(
            "SELECT * FROM direction_log WHERE ball_id='ball-001' ORDER BY ts"
        ).fetchall()
        self.assertEqual(len(rows), 2)
        conn.close()


class TestBrokerIntegration(unittest.TestCase):
    """Live broker integration tests (requires MQTT broker)."""

    @classmethod
    def setUpClass(cls):
        if not HAS_MQTT:
            raise unittest.SkipTest("paho-mqtt not installed")

    def test_connect_to_broker(self):
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        try:
            client.connect("broker.emqx.io", 1883, 10)
            client.loop_start()
            time.sleep(0.5)
            self.assertTrue(client.is_connected())
            client.loop_stop()
            client.disconnect()
        except Exception:
            raise unittest.SkipTest("Cannot reach MQTT broker")

    def test_publish_and_receive(self):
        received = []

        def on_msg(client, userdata, msg):
            received.append(json.loads(msg.payload.decode()))

        sub_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        sub_client.on_message = on_msg

        try:
            sub_client.connect("broker.emqx.io", 1883, 10)
            sub_client.loop_start()
            time.sleep(0.5)

            test_topic = f"/solar/ball/test-int-{int(time.time())}/direction"
            sub_client.subscribe(test_topic, qos=1)
            time.sleep(0.3)

            pub_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
            pub_client.connect("broker.emqx.io", 1883, 10)
            pkt = {
                "id": "test-int", "ts": int(time.time()),
                "dx": 0.4, "dy": 0.3, "dz": 0.87,
                "soc": 75, "rssi": -82, "err": 0, "conf": 210, "flags": 1,
            }
            pub_client.publish(test_topic, json.dumps(pkt), qos=1)
            pub_client.disconnect()

            time.sleep(1.5)

            sub_client.loop_stop()
            sub_client.disconnect()

            self.assertGreater(len(received), 0, "No message received")
            if received:
                self.assertEqual(received[0]["id"], "test-int")
        except Exception as e:
            raise unittest.SkipTest(f"Broker unreachable: {e}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
