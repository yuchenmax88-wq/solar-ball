#!/usr/bin/env python3
"""
Solar Ball OTA Firmware Update Tool
====================================
Pushes firmware binary to the device via serial UART using ESP-IDF OTA protocol.
Also supports triggering OTA via MQTT for remote updates.

Usage:
    python ota_update.py COM3 firmware/solar-ball.bin
    python ota_update.py COM3 --version 1.3.0 firmware/solar-ball.bin
    python ota_update.py --mqtt --ball ball-001 --url http://example.com/ota.bin
"""

import argparse
import json
import os
import struct
import sys
import time
from pathlib import Path

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial required. Install with: pip install pyserial")
    sys.exit(1)

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

BAUDRATE = 115200
CHUNK_SIZE = 4096
MAX_FIRMWARE_SIZE = 3 * 1024 * 1024
SYNC_TIMEOUT = 10
ERASE_TIMEOUT = 60
WRITE_TIMEOUT = 120

OTA_SYNC_PATTERN = b"\xAA\x55\xAA\x55\xAA\x55"
OTA_CMD_BEGIN = 0x01
OTA_CMD_DATA = 0x02
OTA_CMD_FINISH = 0x03
OTA_CMD_ABORT = 0x04
OTA_RESP_OK = 0x00
OTA_RESP_ERR = 0xFF


def find_port(port_hint):
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("Error: No serial ports found")
        sys.exit(1)

    for p in ports:
        if port_hint.upper() in p.device.upper():
            return p.device

    for p in ports:
        if "USB" in p.description or "UART" in p.description:
            return p.device

    print(f"Available ports: {[p.device for p in ports]}")
    print(f"Error: Port '{port_hint}' not found")
    sys.exit(1)


def read_exact(ser, n, timeout):
    buf = b""
    deadline = time.time() + timeout
    while len(buf) < n and time.time() < deadline:
        chunk = ser.read(n - len(buf))
        if chunk:
            buf += chunk
    return buf


def wait_response(ser, expected, timeout):
    buf = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = ser.read(1)
        if chunk:
            buf += chunk
            if expected in buf:
                return True
    return False


def send_ota_begin(ser, version, total_size):
    version_bytes = version.encode("utf-8")[:31] + b"\x00"
    payload = struct.pack("<BI", OTA_CMD_BEGIN, total_size) + version_bytes
    ser.write(payload)
    resp = read_exact(ser, 1, ERASE_TIMEOUT)
    return resp and resp[0] == OTA_RESP_OK


def send_ota_data(ser, chunk, offset):
    payload = struct.pack("<BI", OTA_CMD_DATA, offset) + chunk
    ser.write(payload)
    resp = read_exact(ser, 1, WRITE_TIMEOUT)
    return resp and resp[0] == OTA_RESP_OK


def send_ota_finish(ser):
    ser.write(bytes([OTA_CMD_FINISH]))
    resp = read_exact(ser, 1, WRITE_TIMEOUT)
    return resp and resp[0] == OTA_RESP_OK


def uart_ota(port, firmware_path, version):
    if not os.path.exists(firmware_path):
        print(f"Error: Firmware file not found: {firmware_path}")
        sys.exit(1)

    fw_size = os.path.getsize(firmware_path)
    if fw_size > MAX_FIRMWARE_SIZE:
        print(f"Error: Firmware too large: {fw_size} > {MAX_FIRMWARE_SIZE}")
        sys.exit(1)

    print(f"Firmware: {firmware_path}")
    print(f"Size: {fw_size} bytes ({fw_size / 1024:.1f} KB)")
    print(f"Version: {version}")

    device = find_port(port)
    print(f"Port: {device}")

    ser = serial.Serial(device, BAUDRATE, timeout=1)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    print("Waiting for device sync...")
    ser.write(OTA_SYNC_PATTERN)
    if not wait_response(ser, b"OTA_READY", SYNC_TIMEOUT):
        print("Error: Device did not respond. Ensure the ball is powered on and connected.")
        ser.close()
        sys.exit(1)
    print("Device synced.")

    print(f"Sending OTA begin...")
    if not send_ota_begin(ser, version, fw_size):
        print("Error: OTA begin failed")
        ser.close()
        sys.exit(1)
    print("OTA session started.")

    with open(firmware_path, "rb") as f:
        offset = 0
        while True:
            chunk = f.read(CHUNK_SIZE)
            if not chunk:
                break

            if not send_ota_data(ser, chunk, offset):
                print(f"\nError: OTA write failed at offset {offset}")
                ser.close()
                sys.exit(1)

            offset += len(chunk)
            pct = int(offset / fw_size * 100)
            print(f"\rProgress: {pct}% ({offset}/{fw_size} bytes)", end="", flush=True)

    print()
    print("Sending OTA finish...")
    if not send_ota_finish(ser):
        print("Error: OTA finish failed")
        ser.close()
        sys.exit(1)

    print("OTA update complete. Device will reboot with new firmware.")
    ser.close()


def mqtt_ota(broker, port_mqtt, ball_id, url, version):
    if mqtt is None:
        print("Error: paho-mqtt required. Install with: pip install paho-mqtt")
        sys.exit(1)

    cmd_topic = f"/solar/ball/{ball_id}/cmd"
    payload = json.dumps({
        "cmd": "ota",
        "url": url,
        "version": version
    })

    print(f"MQTT Broker: {broker}:{port_mqtt}")
    print(f"Ball ID: {ball_id}")
    print(f"OTA URL: {url}")
    print(f"Publishing to: {cmd_topic}")
    print(f"Payload: {payload}")

    client = mqtt.Client(client_id="ota-update-tool")
    client.connect(broker, port_mqtt, 60)
    client.loop_start()

    info = client.publish(cmd_topic, payload, qos=1)
    info.wait_for_publish()

    client.loop_stop()
    client.disconnect()

    print("OTA command sent via MQTT. The ball will download and apply the update on its next cycle.")


def main():
    parser = argparse.ArgumentParser(
        description="Solar Ball OTA Firmware Update Tool"
    )
    parser.add_argument(
        "port", nargs="?", default=None,
        help="Serial port (e.g., COM3 or /dev/ttyUSB0)"
    )
    parser.add_argument(
        "firmware", nargs="?", default=None,
        help="Path to firmware binary (solar-ball.bin)"
    )
    parser.add_argument(
        "--version", default="1.3.0",
        help="Firmware version string (default: 1.3.0)"
    )
    parser.add_argument(
        "--mqtt", action="store_true",
        help="Trigger OTA via MQTT instead of serial"
    )
    parser.add_argument(
        "--broker", default="broker.emqx.io",
        help="MQTT broker hostname (default: broker.emqx.io)"
    )
    parser.add_argument(
        "--mqtt-port", type=int, default=1883,
        help="MQTT broker port (default: 1883)"
    )
    parser.add_argument(
        "--ball", default="ball-001",
        help="Ball ID for MQTT OTA (default: ball-001)"
    )
    parser.add_argument(
        "--url", default=None,
        help="Firmware download URL for MQTT-triggered OTA"
    )

    args = parser.parse_args()

    if args.mqtt:
        if not args.url:
            print("Error: --url is required for MQTT OTA")
            sys.exit(1)
        mqtt_ota(args.broker, args.mqtt_port, args.ball, args.url, args.version)
    else:
        if not args.port or not args.firmware:
            parser.print_help()
            sys.exit(1)
        uart_ota(args.port, args.firmware, args.version)


if __name__ == "__main__":
    main()
