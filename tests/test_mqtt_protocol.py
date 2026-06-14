"""
Unit tests for mqtt_protocol.h — MQTT topic building and JSON serialization.
Ports the C inline functions to Python for verification.
"""

import json
import sys

# ---- Replicate C logic ----

MQTT_TOPIC_PREFIX = "/solar/ball/"


def build_mqtt_topic(ball_id: str) -> str:
    """snprintf(out, out_len, "%s%s/direction", MQTT_TOPIC_PREFIX, ball_id)"""
    return f"{MQTT_TOPIC_PREFIX}{ball_id}/direction"


def serialize_direction(pkt: dict) -> str:
    """
    snprintf(buf, buf_len,
        '{"id":"%s","ts":%lu,"dx":%.4f,"dy":%.4f,"dz":%.4f,'
        '"soc":%u,"rssi":%d,"err":%u,"conf":%u,"flags":%u}',
        ...)
    """
    return (
        f'{{"id":"{pkt["id"]}","ts":{pkt["ts"]},'
        f'"dx":{pkt["dx"]:.4f},"dy":{pkt["dy"]:.4f},'
        f'"dz":{pkt["dz"]:.4f},'
        f'"soc":{pkt["soc"]},"rssi":{pkt["rssi"]},'
        f'"err":{pkt.get("err",0)},"conf":{pkt.get("conf",0)},'
        f'"flags":{pkt.get("flags",0)}}}'
    )


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


def test_topic_building():
    """Test MQTT topic format: /solar/ball/{id}/direction"""
    topic = build_mqtt_topic("ball-001")
    check("topic format", topic == "/solar/ball/ball-001/direction", f"got: {topic}")

    topic = build_mqtt_topic("field-07")
    check("topic custom id", topic == "/solar/ball/field-07/direction", f"got: {topic}")

    topic = build_mqtt_topic("")
    check("topic empty id", topic == "/solar/ball//direction", f"got: {topic}")

    # Topic length must not exceed MQTT_TOPIC_MAX_LEN (64)
    long_id = "x" * 30
    topic = build_mqtt_topic(long_id)
    check("topic under 64 bytes", len(topic) <= 64, f"length={len(topic)}")


def test_serialize_valid():
    """Serialize a normal packet and validate JSON"""
    pkt = {
        "id": "ball-001",
        "ts": 1718000000,
        "dx": 0.3215,
        "dy": -0.1478,
        "dz": 0.9352,
        "soc": 87,
        "rssi": -89,
        "err": 0,
        "conf": 220,
        "flags": 9,
    }
    s = serialize_direction(pkt)
    parsed = json.loads(s)
    check("id matches", parsed["id"] == "ball-001")
    check("ts matches", parsed["ts"] == 1718000000)
    check("dx matches", abs(parsed["dx"] - 0.3215) < 0.0001, f"got {parsed['dx']}")
    check("dy matches", abs(parsed["dy"] - (-0.1478)) < 0.0001)
    check("dz matches", abs(parsed["dz"] - 0.9352) < 0.0001)
    check("soc matches", parsed["soc"] == 87)
    check("rssi matches", parsed["rssi"] == -89)
    check("err matches", parsed["err"] == 0)
    check("conf matches", parsed["conf"] == 220)
    check("flags matches", parsed["flags"] == 9)


def test_serialize_boundary_values():
    """Test extremes: soc=0, soc=100, rssi=-999, large ts"""
    pkt = {
        "id": "test",
        "ts": 4294967295,  # max uint32
        "dx": 1.0,
        "dy": 1.0,
        "dz": 1.0,
        "soc": 0,
        "rssi": -999,
    }
    s = serialize_direction(pkt)
    parsed = json.loads(s)
    check("max ts 32bit", parsed["ts"] == 4294967295)
    check("soc=0", parsed["soc"] == 0)

    pkt["soc"] = 100
    pkt["ts"] = 0
    pkt["rssi"] = -40
    pkt["dx"] = -1.0
    pkt["dy"] = -1.0
    pkt["dz"] = -1.0
    s = serialize_direction(pkt)
    parsed = json.loads(s)
    check("soc=100", parsed["soc"] == 100)
    check("ts=0", parsed["ts"] == 0)
    check("rssi=-40", parsed["rssi"] == -40)
    check("neg dx", abs(parsed["dx"] - (-1.0)) < 0.001)


def test_serialize_float_precision():
    """.4f format should produce 4 decimal places"""
    pkt = {
        "id": "p",
        "ts": 1,
        "dx": 1.23456789,
        "dy": -0.123456789,
        "dz": 0.00009999,
        "soc": 50,
        "rssi": -50,
    }
    s = serialize_direction(pkt)
    parsed = json.loads(s)
    # dx should round to 1.2346
    check("dx 4dp", abs(parsed["dx"] - 1.2346) < 0.0001, f"got {parsed['dx']}")
    # dy should round to -0.1235
    check("dy 4dp", abs(parsed["dy"] - (-0.1235)) < 0.0001, f"got {parsed['dy']}")
    # dz 0.0001
    check("dz 4dp", abs(parsed["dz"] - 0.0001) < 0.0001, f"got {parsed['dz']}")


def test_serialize_small_float():
    """Very small floats (.4f) should not go to scientific notation"""
    pkt = {
        "id": "x",
        "ts": 1,
        "dx": 0.0000123,  # .4f -> 0.0000
        "dy": 0.0000123,
        "dz": 0.0000123,
        "soc": 50,
        "rssi": -50,
    }
    s = serialize_direction(pkt)
    check("no scientific notation", s.find("e-") == -1)
    check("no uppercase E-", s.find("E-") == -1)


def test_payload_max_len():
    """Verify serialized payload fits in MQTT_PAYLOAD_MAX_LEN (256 bytes)"""
    # Normal packet
    pkt = {
        "id": "ball-001",
        "ts": 1718000000,
        "dx": 0.3215,
        "dy": -0.1478,
        "dz": 0.9352,
        "soc": 87,
        "rssi": -89,
    }
    s = serialize_direction(pkt)
    check("payload under 256", len(s) <= 256, f"length={len(s)} (normal packet)")

    # Worst case: all 16-char id, max-length numbers
    pkt = {
        "id": "X" * 16,
        "ts": 4294967295,
        "dx": -1.0000,
        "dy": -1.0000,
        "dz": -1.0000,
        "soc": 100,
        "rssi": -999,
    }
    s = serialize_direction(pkt)
    check("payload under 256 worst", len(s) <= 256, f"length={len(s)} (worst case)")


# ---- Run ----

if __name__ == "__main__":
    print("=== mqtt_protocol.h serialization tests ===\n")

    test_topic_building()
    test_serialize_valid()
    test_serialize_boundary_values()
    test_serialize_float_precision()
    test_serialize_small_float()
    test_payload_max_len()

    print(f"\n--- Results: {passed} passed, {failed} failed ---")
    sys.exit(0 if failed == 0 else 1)
