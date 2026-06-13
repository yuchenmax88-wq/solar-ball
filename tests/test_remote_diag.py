"""
Unit tests for remote_diag.c diagnostic protocol serialization.
"""
import json
import sys

def remote_diag_serialize(snap):
    """Port of remote_diag_serialize() from remote_diag.c"""
    return (
        f'{{"id":"{snap["id"]}","uptime":{snap["uptime"]},'
        f'"scans":{snap["scans"]},'
        f'"pub_ok":{snap["pub_ok"]},"pub_fail":{snap["pub_fail"]},'
        f'"err":{snap["err"]},"conf":{snap["conf"]},'
        f'"flags":{snap["flags"]},'
        f'"bat_mv":{snap["bat_mv"]},"rssi":{snap["rssi"]},'
        f'"faults":{snap["faults"]},"tz":{snap["tz"]}}}'
    )


def remote_diag_build_snapshot(ball_id, uptime=0, scans=0,
                                pub_ok=0, pub_fail=0,
                                err=0, conf=255, flags=1,
                                bat_mv=3700, rssi=-89,
                                faults=0, tz=8):
    """Port of remote_diag_build_snapshot()"""
    return {
        "id": ball_id,
        "uptime": uptime,
        "scans": scans,
        "pub_ok": pub_ok,
        "pub_fail": pub_fail,
        "err": err,
        "conf": conf,
        "flags": flags,
        "bat_mv": bat_mv,
        "rssi": rssi,
        "faults": faults,
        "tz": tz,
    }


passed = 0
failed = 0

def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
    else:
        failed += 1
        print(f"  FAIL: {name} {detail}")


def test_serialize_valid():
    snap = remote_diag_build_snapshot("ball-001", uptime=3600, scans=720,
                                       pub_ok=718, pub_fail=2,
                                       err=0x0000, conf=220, flags=0x0F)
    s = remote_diag_serialize(snap)
    d = json.loads(s)
    check("valid id", d["id"] == "ball-001")
    check("uptime", d["uptime"] == 3600)
    check("scans", d["scans"] == 720)
    check("pub_ok", d["pub_ok"] == 718)
    check("pub_fail", d["pub_fail"] == 2)
    check("err", d["err"] == 0)
    check("conf", d["conf"] == 220)
    check("flags", d["flags"] == 0x0F)
    check("rssi", d["rssi"] == -89)


def test_serialize_with_faults():
    snap = remote_diag_build_snapshot("ball-007",
                                       err=0x0111, conf=30, flags=0x02,
                                       faults=5, rssi=-999)
    s = remote_diag_serialize(snap)
    d = json.loads(s)
    check("fault err", d["err"] == 0x0111)
    check("fault conf", d["conf"] == 30)
    check("fault count", d["faults"] == 5)
    check("no signal rssi", d["rssi"] == -999)


def test_serialize_boundaries():
    snap = remote_diag_build_snapshot("test",
                                       uptime=4294967295, scans=4294967295,
                                       pub_ok=0, pub_fail=0,
                                       conf=0, flags=0, rssi=0)
    s = remote_diag_serialize(snap)
    d = json.loads(s)
    check("max uptime", d["uptime"] == 4294967295)
    check("zero conf", d["conf"] == 0)


def test_serialize_long_id():
    snap = remote_diag_build_snapshot("field-station-07", uptime=0)
    s = remote_diag_serialize(snap)
    d = json.loads(s)
    check("long id", d["id"] == "field-station-07")


def test_serialize_negative_tz():
    snap = remote_diag_build_snapshot("ball-001", tz=-5)
    s = remote_diag_serialize(snap)
    d = json.loads(s)
    check("negative tz", d["tz"] == -5)


def test_json_roundtrip():
    """Serialize → parse → serialize → parse should be stable"""
    snap = remote_diag_build_snapshot("ball-001",
                                       uptime=12345, scans=2468,
                                       pub_ok=2460, pub_fail=8,
                                       err=0x0004, conf=180, flags=0x09,
                                       bat_mv=3850, rssi=-75, faults=2, tz=8)
    s1 = remote_diag_serialize(snap)
    d1 = json.loads(s1)
    s2 = remote_diag_serialize({
        "id": d1["id"], "uptime": d1["uptime"],
        "scans": d1["scans"], "pub_ok": d1["pub_ok"],
        "pub_fail": d1["pub_fail"], "err": d1["err"],
        "conf": d1["conf"], "flags": d1["flags"],
        "bat_mv": d1["bat_mv"], "rssi": d1["rssi"],
        "faults": d1["faults"], "tz": d1["tz"]
    })
    d2 = json.loads(s2)
    check("roundtrip id", d1["id"] == d2["id"])
    check("roundtrip conf", d1["conf"] == d2["conf"])
    check("roundtrip err", d1["err"] == d2["err"])


if __name__ == "__main__":
    print("=== remote_diag protocol tests ===\n")
    test_serialize_valid()
    test_serialize_with_faults()
    test_serialize_boundaries()
    test_serialize_long_id()
    test_serialize_negative_tz()
    test_json_roundtrip()
    print(f"\n--- Results: {passed} passed, {failed} failed ---")
    sys.exit(0 if failed == 0 else 1)
