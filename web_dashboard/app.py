#!/usr/bin/env python3
"""
Solar Ball Web Configuration Dashboard
=======================================
Flask-based web dashboard for monitoring and configuring Solar Ball devices.
Connects to MQTT broker for real-time data and provides:
 - Live direction/status dashboard per ball
 - Historical data logging (SQLite)
 - OTA firmware trigger
 - Multi-ball overview
 - REST API for all operations

Usage:
    python app.py
    python app.py --port 8080 --broker broker.emqx.io
    python app.py --config config.json
"""

import argparse
import json
import math
import os
import queue
import re
import sqlite3
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Error: paho-mqtt required. Install: pip install paho-mqtt")
    sys.exit(1)

from flask import (
    Flask, Response, jsonify, render_template, request, stream_with_context
)

APP_DIR = Path(__file__).resolve().parent
DB_PATH = APP_DIR / "data" / "history.db"
DEFAULT_CONFIG_PATH = APP_DIR / "config.json"

app = Flask(__name__, template_folder=str(APP_DIR / "templates"),
            static_folder=str(APP_DIR / "static"))

sse_queue = queue.Queue(maxsize=256)
latest_data = {}
latest_lock = threading.Lock()
mqtt_client = None
start_time = time.time()


def init_db():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(DB_PATH))
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("""
        CREATE TABLE IF NOT EXISTS direction_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ball_id TEXT NOT NULL,
            ts INTEGER NOT NULL,
            dx REAL, dy REAL, dz REAL,
            azimuth REAL, elevation REAL,
            soc INTEGER, rssi INTEGER,
            error_mask INTEGER,
            confidence INTEGER,
            flags INTEGER,
            recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.execute("""
        CREATE INDEX IF NOT EXISTS idx_direction_ball_ts
        ON direction_log(ball_id, ts)
    """)
    conn.commit()
    return conn


def store_reading(data: dict):
    conn = sqlite3.connect(str(DB_PATH))
    dx, dy, dz = data.get("dx", 0), data.get("dy", 0), data.get("dz", 0)
    az, elev = vector_to_angles(dx, dy, dz)
    conn.execute("""
        INSERT INTO direction_log
        (ball_id, ts, dx, dy, dz, azimuth, elevation,
         soc, rssi, error_mask, confidence, flags)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        data.get("id", "?"), data.get("ts", 0),
        dx, dy, dz, az, elev,
        data.get("soc", 0), data.get("rssi", 0),
        data.get("err", 0), data.get("conf", 0), data.get("flags", 0),
    ))
    conn.commit()
    conn.close()


def vector_to_angles(dx, dy, dz):
    elev = math.degrees(math.asin(max(-1.0, min(1.0, dz))))
    az = math.degrees(math.atan2(dx, dy))
    if az < 0:
        az += 360.0
    return round(az, 1), round(elev, 1)


def panel_orientation(az, elev):
    return round(az, 1), round(90.0 - elev, 1)


def decode_faults(error_mask):
    faults = {
        0x0001: "SENSOR_OPEN", 0x0002: "SENSOR_SHORT", 0x0004: "SATURATED",
        0x0008: "ADC_ERR", 0x0010: "MODEM_ERR", 0x0020: "MQTT_ERR",
        0x0040: "LOW_BAT", 0x0080: "NO_CALIB", 0x0100: "OVERCAST", 0x0200: "NIGHT",
    }
    active = []
    for mask, name in faults.items():
        if error_mask & mask:
            active.append(name)
    return active


def decode_flags(flags):
    result = []
    if flags & 0x01: result.append("DIR_OK")
    if flags & 0x02: result.append("OVERCAST")
    if flags & 0x04: result.append("NIGHT")
    if flags & 0x08: result.append("CALIB")
    if flags & 0x10: result.append("BASELINE")
    return result


BALL_ID_RE = re.compile(r'^[a-zA-Z0-9_-]+$')


def validate_ball_id(ball_id):
    return bool(BALL_ID_RE.match(ball_id))


def on_mqtt_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        topic = userdata.get("topic", "/solar/ball/+/direction")
        client.subscribe(topic, qos=1)
        print(f"[MQTT] Connected, subscribed to {topic}")
    else:
        print(f"[MQTT] Connection failed: {reason_code}")


def on_mqtt_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        return

    ball_id = data.get("id", "?")
    az, elev = vector_to_angles(
        data.get("dx", 0), data.get("dy", 0), data.get("dz", 0)
    )
    data["azimuth"] = az
    data["elevation"] = elev
    data["panel_az"], data["panel_tilt"] = panel_orientation(az, elev)
    data["fault_list"] = decode_faults(data.get("err", 0))
    data["flag_list"] = decode_flags(data.get("flags", 0))
    data["mag"] = round(math.sqrt(
        data.get("dx", 0)**2 + data.get("dy", 0)**2 + data.get("dz", 0)**2
    ), 4)
    data["vector_ok"] = 0.99 < data["mag"] < 1.01

    with latest_lock:
        latest_data[ball_id] = data

    try:
        sse_queue.put_nowait(json.dumps({"ball_id": ball_id, "data": data}))
    except queue.Full:
        pass

    try:
        store_reading(data)
    except Exception as e:
        print(f"[DB] Failed to store reading: {e}")


def start_mqtt(broker, port, topic):
    global mqtt_client
    mqtt_client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        userdata={"topic": topic}
    )
    mqtt_client.on_connect = on_mqtt_connect
    mqtt_client.on_message = on_mqtt_message

    while True:
        try:
            mqtt_client.connect(broker, port, 60)
            mqtt_client.loop_forever()
        except Exception as e:
            print(f"[MQTT] Error: {e}, retrying in 10s...")
            time.sleep(10)


def sse_stream():
    while True:
        try:
            msg = sse_queue.get(timeout=30)
            yield f"data: {msg}\n\n"
        except queue.Empty:
            yield f"data: {json.dumps({'ping': True})}\n\n"


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/balls")
def api_balls():
    with latest_lock:
        balls = {}
        for bid, data in latest_data.items():
            balls[bid] = {
                "id": bid,
                "ts": data.get("ts", 0),
                "azimuth": data.get("azimuth", 0),
                "elevation": data.get("elevation", 0),
                "soc": data.get("soc", 0),
                "rssi": data.get("rssi", 0),
                "confidence": data.get("conf", 0),
                "error_mask": data.get("err", 0),
                "flags": data.get("flags", 0),
                "fault_list": data.get("fault_list", []),
                "flag_list": data.get("flag_list", []),
                "vector_ok": data.get("vector_ok", False),
                "age_seconds": int(time.time()) - int(data.get("ts", 0)),
            }
        return jsonify(list(balls.values()))


@app.route("/api/ball/<ball_id>")
def api_ball(ball_id):
    with latest_lock:
        data = latest_data.get(ball_id)
        if data is None:
            return jsonify({"error": "No data"}), 404
        return jsonify(dict(data))


@app.route("/api/history/<ball_id>")
def api_history(ball_id):
    try:
        limit = min(int(request.args.get("limit", 100)), 1000)
    except ValueError:
        limit = 100

    conn = sqlite3.connect(str(DB_PATH))
    conn.row_factory = sqlite3.Row
    rows = conn.execute("""
        SELECT * FROM direction_log
        WHERE ball_id = ?
        ORDER BY ts DESC
        LIMIT ?
    """, (ball_id, limit)).fetchall()
    conn.close()
    return jsonify([dict(r) for r in rows])


@app.route("/api/ota", methods=["POST"])
def api_ota_trigger():
    if mqtt_client is None:
        return jsonify({"error": "MQTT not connected"}), 503

    body = request.get_json(silent=True) or {}
    ball_id = body.get("ball_id", "ball-001")
    url = body.get("url")
    version = body.get("version", "1.3.0")

    if not validate_ball_id(ball_id):
        return jsonify({"error": "Invalid ball_id"}), 400
    if not url:
        return jsonify({"error": "url is required"}), 400

    topic = f"/solar/ball/{ball_id}/cmd"
    payload = json.dumps({"cmd": "ota", "url": url, "version": version})

    info = mqtt_client.publish(topic, payload, qos=1)
    info.wait_for_publish(timeout=5)

    return jsonify({
        "ok": True,
        "topic": topic,
        "ball_id": ball_id,
        "url": url,
        "version": version,
    })


@app.route("/api/stats")
def api_stats():
    conn = sqlite3.connect(str(DB_PATH))
    total = conn.execute("SELECT COUNT(*) FROM direction_log").fetchone()[0]
    balls = conn.execute(
        "SELECT ball_id, COUNT(*) as cnt, MAX(ts) as last_ts FROM direction_log GROUP BY ball_id"
    ).fetchall()
    conn.close()

    return jsonify({
        "total_readings": total,
        "balls": [
            {"id": b[0], "count": b[1], "last_ts": b[2]}
            for b in balls
        ],
        "uptime_seconds": int(time.time() - start_time),
    })


@app.route("/api/publish", methods=["POST"])
def api_publish():
    if mqtt_client is None:
        return jsonify({"error": "MQTT not connected"}), 503

    body = request.get_json(silent=True) or {}
    ball_id = body.get("ball_id", "ball-001")

    if not validate_ball_id(ball_id):
        return jsonify({"error": "Invalid ball_id"}), 400

    topic = f"/solar/ball/{ball_id}/cmd"
    cmd = body.get("cmd", {})
    payload = json.dumps(cmd)

    info = mqtt_client.publish(topic, payload, qos=1)
    info.wait_for_publish(timeout=5)

    return jsonify({"ok": True, "topic": topic})


@app.route("/stream")
def stream():
    return Response(
        stream_with_context(sse_stream()),
        mimetype="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "X-Accel-Buffering": "no",
        }
    )


def main():
    parser = argparse.ArgumentParser(description="Solar Ball Web Dashboard")
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--broker", default="broker.emqx.io")
    parser.add_argument("--mqtt-port", type=int, default=1883)
    parser.add_argument("--topic", default="/solar/ball/+/direction")
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    init_db()

    mqtt_thread = threading.Thread(
        target=start_mqtt,
        args=(args.broker, args.mqtt_port, args.topic),
        daemon=True
    )
    mqtt_thread.start()

    print(f"")
    print(f"  Solar Ball Web Dashboard")
    print(f"  http://{args.host}:{args.port}")
    print(f"  MQTT broker: {args.broker}:{args.mqtt_port}")
    print(f"  Topic: {args.topic}")
    print(f"")

    app.run(host=args.host, port=args.port, debug=args.debug, threaded=True)


if __name__ == "__main__":
    main()
