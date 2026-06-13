#!/usr/bin/env python3
"""
Solar Ball MQTT Receiver
========================
Subscribes to Solar Ball direction updates and converts the unit vector
to azimuth/elevation for solar array tracking.

Usage:
    python mqtt_receiver.py                  # connects to public test broker
    python mqtt_receiver.py --broker 192.168.1.1 --ball ball-001

Data flow:
    /solar/ball/{id}/direction  →  JSON parse  →  (dx,dy,dz)  →  azimuth,elevation
"""

import json
import math
import sys
import time
import paho.mqtt.client as mqtt

# ---------- Default config ----------
MQTT_BROKER = "broker.emqx.io"
MQTT_PORT = 1883
BALL_ID = "ball-001"
TOPIC = f"/solar/ball/{BALL_ID}/direction"


def vector_to_angles(dx: float, dy: float, dz: float):
    """
    Convert Solar Ball direction unit vector to azimuth + elevation.
    
    Solar Ball coordinate system:
        x = east, y = north, z = up
    
    Returns:
        azimuth: degrees from north, clockwise (0=N, 90=E, 180=S, 270=W)
        elevation: degrees above horizon (0=horizon, 90=zenith)
    """
    # Elevation: angle above horizontal plane
    elev = math.degrees(math.asin(max(-1.0, min(1.0, dz))))
    
    # Azimuth: angle from north in horizontal plane
    az = math.degrees(math.atan2(dx, dy))  # atan2(east, north)
    if az < 0:
        az += 360.0
    
    return az, elev


def compute_panel_orientation(az: float, elev: float, panel_lat: float = None):
    """
    Compute solar panel tilt and rotation from sun direction.
    
    Most solar arrays:
    - Rotate around vertical axis (azimuth tracking) → set to sun_azimuth
    - Tilt around horizontal axis (elevation tracking) → set to 90° - sun_elevation
    
    For a flat panel:
    - panel_azimuth = sun_azimuth (face the sun)
    - panel_tilt = 90° - sun_elevation (tilt up toward sun)
    """
    panel_azimuth = az
    panel_tilt = 90.0 - elev  # 90° = flat, 0° = vertical
    
    return panel_azimuth, panel_tilt


def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print(f"Connected to {MQTT_BROKER}")
        client.subscribe(TOPIC, qos=1)
        print(f"Subscribed to {TOPIC}")
    else:
        print(f"Connection failed: {reason_code}")


def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        print(f"Invalid JSON: {msg.payload}")
        return

    ball_id = data.get("id", "?")
    dx = data.get("dx", 0.0)
    dy = data.get("dy", 0.0)
    dz = data.get("dz", 0.0)
    soc = data.get("soc", 0)
    rssi = data.get("rssi", 0)
    
    az, elev = vector_to_angles(dx, dy, dz)
    panel_az, panel_tilt = compute_panel_orientation(az, elev)
    
    # Verify unit vector validity
    mag = math.sqrt(dx*dx + dy*dy + dz*dz)
    valid = "OK" if 0.99 < mag < 1.01 else "BAD"
    
    print(f"[{ball_id}] Sun: az={az:6.1f}° elev={elev:+5.1f}°  "
          f"Panel: az={panel_az:6.1f}° tilt={panel_tilt:4.1f}°  "
          f"SOC={soc}% RSSI={rssi}dBm  vector:{valid}")

    # Here you would send panel_azimuth and panel_tilt to your
    # solar array motor controller via serial/Modbus/CAN/etc.


def main():
    global MQTT_BROKER, MQTT_PORT, BALL_ID, TOPIC
    
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--broker" and i + 1 < len(args):
            MQTT_BROKER = args[i + 1]; i += 2
        elif args[i] == "--port" and i + 1 < len(args):
            MQTT_PORT = int(args[i + 1]); i += 2
        elif args[i] == "--ball" and i + 1 < len(args):
            BALL_ID = args[i + 1]
            TOPIC = f"/solar/ball/{BALL_ID}/direction"
            i += 2
        else:
            i += 1

    print(f"Solar Ball MQTT Receiver")
    print(f"  Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"  Topic:  {TOPIC}")
    print(f"  Format: JSON {{dx,dy,dz}} → azimuth/elevation → panel orientation")
    print()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nDisconnected")
    except Exception as e:
        print(f"Error: {e}")


if __name__ == "__main__":
    main()
