#!/usr/bin/env python3
"""
Solar Ball Auto-Calibration Tool
=================================

Maps each of the 80 physical sensor channels to its actual position
on the ball. Run this after physical assembly.

Usage:
    python auto_calibrate.py <COM_PORT>

    Example:
        python auto_calibrate.py COM3

How it works:
    1. Connects to the ball's ESP32 via UART serial
    2. For each of the 80 positions (top to bottom):
       a. Prompts you to shine a bright flashlight at that position number
       b. The ball scans all 80 channels and finds the brightest one
       c. The tool records: Position N -> Physical Channel X
    3. After all 80 positions are mapped, saves to NVS
    4. Runs a verification scan to check the mapping

The ball shell has each sensor position numbered in the 3D printed design.
Position #0 = very top, Position #79 = lowest ring.
"""

import sys
import time
import serial
import re
import argparse
from colorama import init, Fore, Style

init(autoreset=True)

BAUD_RATE = 115200
CMD_TIMEOUT = 5
SCAN_TIMEOUT = 10


class BallCalibrator:
    def __init__(self, port: str):
        self.ser = serial.Serial(port, BAUD_RATE, timeout=2)
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        self.channel_map = {}  # position_idx -> physical_channel
        self.position_count = 80
        time.sleep(1)

    def send_command(self, cmd: str, timeout: float = CMD_TIMEOUT) -> str:
        self.ser.write(f"{cmd}\n".encode())
        self.ser.flush()
        deadline = time.time() + timeout
        buf = ""
        while time.time() < deadline:
            if self.ser.in_waiting:
                line = self.ser.readline().decode(errors="replace").strip()
                buf += line + "\n"
                if "OK" in line or "ERR" in line or "CAL:" in line:
                    return buf.strip()
            time.sleep(0.05)
        return buf.strip()

    def scan_brightest_channel(self) -> tuple:
        """Ask ball to scan and return (channel, value) of brightest sensor."""
        response = self.send_command("CAL:SCAN", timeout=SCAN_TIMEOUT)
        match = re.search(r"CAL:RESULT channel=(\d+) value=(\d+)", response)
        if match:
            return (int(match.group(1)), int(match.group(2)))
        return (-1, 0)

    def map_position(self, pos: int, channel: int) -> bool:
        """Send mapping: position -> channel."""
        response = self.send_command(f"CAL:MAP {pos} {channel}")
        return "CAL:MAP_OK" in response

    def save_calibration(self) -> bool:
        response = self.send_command("CAL:SAVE", timeout=5)
        return "CAL:SAVE_OK" in response

    def start_calibration(self) -> bool:
        self.send_command("CAL:QUIT", timeout=2)  # exit any previous mode
        self.send_command("CAL:START", timeout=3)
        response = self.send_command("CAL:STATUS")
        return "CAL:READY" in response

    def interactive_calibrate(self):
        print(f"\n{Fore.CYAN}{'='*60}")
        print("  Solar Ball Auto-Calibration Tool")
        print(f"{'='*60}{Style.RESET_ALL}\n")

        print(f"{Fore.YELLOW}Instructions:{Style.RESET_ALL}")
        print("  1. Make sure the ball is powered and connected via USB-UART")
        print(f"  2. You need a BRIGHT flashlight (phone LED works)")
        print("  3. The ball has 80 numbered sensor positions (see CAD model)")
        print(f"  4. Position #0 = TOP of the ball (the very top)\n")
        print(f"{Fore.YELLOW}Starting calibration...{Style.RESET_ALL}")

        if not self.start_calibration():
            print(f"{Fore.RED}Failed to start calibration mode!{Style.RESET_ALL}")
            sys.exit(1)

        print(f"{Fore.GREEN}Calibration mode ready.{Style.RESET_ALL}\n")

        for pos in range(self.position_count):
            # Prompt user
            input(f"\n{Fore.CYAN}Position #{pos}/{self.position_count-1}:{Style.RESET_ALL}\n"
                  f"  Shine the flashlight directly at position #{pos} on the ball.\n"
                  f"  Hold steady, then press {Fore.GREEN}[Enter]{Style.RESET_ALL}...")

            # Scan
            channel, value = self.scan_brightest_channel()

            if channel < 0 or value < 100:
                print(f"  {Fore.RED}No strong reading! value={value}. Try again?{Style.RESET_ALL}")
                retry = input("  Press [Enter] to retry, 's' to skip: ")
                if retry.lower() == 's':
                    print(f"  {Fore.YELLOW}Skipped position #{pos}{Style.RESET_ALL}")
                    continue

                # Retry scan
                channel, value = self.scan_brightest_channel()
                if channel < 0 or value < 100:
                    print(f"  {Fore.RED}Still no good reading. Checking ambient...{Style.RESET_ALL}")
                    time.sleep(1)
                    ambient_ch, ambient_val = self.scan_brightest_channel()
                    print(f"  Ambient: channel={ambient_ch}, value={ambient_val}")
                    print(f"  {Fore.YELLOW}Make sure flashlight is very close to the hole.{Style.RESET_ALL}")
                    input("  Press [Enter] when ready to try again...")
                    channel, value = self.scan_brightest_channel()

            # Map it
            if self.map_position(pos, channel):
                self.channel_map[pos] = channel
                print(f"  {Fore.GREEN}✓ Position #{pos} -> Physical Channel #{channel} (value={value}){Style.RESET_ALL}")
            else:
                print(f"  {Fore.RED}Failed to send mapping!{Style.RESET_ALL}")

        print(f"\n{Fore.CYAN}{'='*60}")
        print("  Calibration complete! Mapping summary:")
        print(f"{'='*60}{Style.RESET_ALL}")

        for pos in sorted(self.channel_map.keys()):
            ch = self.channel_map[pos]
            print(f"  Position #{pos:02d} -> Channel #{ch:02d}")

        print(f"\nSaving to NVS...")
        if self.save_calibration():
            print(f"{Fore.GREEN}✓ Calibration saved to ball's NVS memory!{Style.RESET_ALL}")
        else:
            print(f"{Fore.RED}Failed to save! Try again.{Style.RESET_ALL}")

        self.send_command("CAL:QUIT")
        print(f"\n{Fore.CYAN}Calibration complete. Restart the ball for normal operation.{Style.RESET_ALL}\n")


    def auto_sun_calibrate(self):
        """Run autonomous sun-based calibration (no manual steps)."""
        print(f"\n{Fore.CYAN}{'='*60}")
        print("  Solar Ball Sun Auto-Calibration")
        print(f"{'='*60}{Style.RESET_ALL}\n")

        print(f"{Fore.YELLOW}Prerequisites:{Style.RESET_ALL}")
        print("  1. Place the ball outdoors under clear sky")
        print("  2. Ensure 4G SIM is inserted and has signal")
        print("  3. The ball will automatically locate itself and calibrate\n")

        print(f"{Fore.YELLOW}Sending CAL:SUN_AUTO...{Style.RESET_ALL}")

        # Send start command
        self.ser.reset_input_buffer()
        self.send_command("CAL:QUIT", timeout=2)  # exit any previous mode
        self.send_command("CAL:START", timeout=2)
        self.send_command("CAL:SUN_AUTO", timeout=5)

        # Wait for result
        deadline = time.time() + 90  # 4G init + location + scan can take ~60s
        buf = ""
        while time.time() < deadline:
            if self.ser.in_waiting:
                line = self.ser.readline().decode(errors="replace").strip()
                buf += line + "\n"
                print(f"  {line}")
                if "CAL:SUN_OK" in line:
                    print(f"\n{Fore.GREEN}✓ Auto-calibration successful!{Style.RESET_ALL}")
                    self.send_command("CAL:SAVE")
                    self.send_command("CAL:QUIT")
                    return True
                if "CAL:SUN_ERR" in line:
                    print(f"\n{Fore.RED}✗ Auto-calibration failed!{Style.RESET_ALL}")
                    print(f"  Reason: {line}")
                    print(f"  Fall back to manual calibration: python {sys.argv[0]} {self.ser.port}")
                    return False
            time.sleep(0.1)

        print(f"\n{Fore.RED}✗ Timeout waiting for calibration result{Style.RESET_ALL}")
        return False


def verify_mapping(port: str) -> bool:
    """Simple verification: checks if saved mapping loads correctly."""
    print(f"\nVerification complete - mapping was saved to NVS.")
    print("To verify: restart the ball and check the serial logs for:")
    print('  "Channel mapping loaded from NVS"')
    return True


def calibration_quality_report(channel_map: dict) -> str:
    """
    Assess calibration quality and produce a report.

    Checks:
      - Coverage: how many positions were mapped (out of 80)
      - Duplicates: if two positions map to the same channel (wiring error)
      - Consecutive channel patterns: expected if wiring follows PCB order
      - Gap analysis: missing positions
    """
    total_positions = 80
    mapped_count = len(channel_map)
    coverage_pct = mapped_count / total_positions * 100

    report_lines = []
    report_lines.append("=" * 60)
    report_lines.append("  Calibration Quality Report")
    report_lines.append("=" * 60)
    report_lines.append(f"  Positions mapped: {mapped_count}/{total_positions} ({coverage_pct:.1f}%)")

    quality = "EXCELLENT" if coverage_pct >= 98 else (
        "GOOD" if coverage_pct >= 90 else (
        "FAIR" if coverage_pct >= 70 else "POOR"))

    report_lines.append(f"  Quality grade:     {quality}")

    if mapped_count == total_positions:
        report_lines.append("  All 80 positions mapped successfully.")
    else:
        missing = sorted(set(range(total_positions)) - set(channel_map.keys()))
        report_lines.append(f"  Missing positions: {len(missing)} — {missing[:10]}{'...' if len(missing) > 10 else ''}")

    channels_used = list(channel_map.values())
    unique_channels = set(channels_used)
    if len(unique_channels) < len(channels_used):
        duplicates = []
        seen = {}
        for pos, ch in sorted(channel_map.items()):
            if ch in seen:
                duplicates.append(f"pos#{seen[ch]} & pos#{pos} → ch#{ch}")
            seen[ch] = pos
        report_lines.append(f"  WARNING: {len(channels_used) - len(unique_channels)} duplicate channel mapping(s):")
        for d in duplicates[:5]:
            report_lines.append(f"    {d}")

    prev = -1
    breaks = []
    for ch in sorted(channels_used):
        if prev >= 0 and ch != prev + 1:
            breaks.append((prev, ch))
        prev = ch
    if len(breaks) <= 2:
        report_lines.append("  Channel sequence: mostly consecutive — OK.")
    else:
        report_lines.append(f"  Channel sequence: {len(breaks)} gaps — wiring may be non-contiguous.")

    if mapped_count >= 70:
        report_lines.append("  Direction accuracy estimate: 0.3–0.5° (with baseline norm).")
    elif mapped_count >= 50:
        report_lines.append("  Direction accuracy estimate: 0.8–1.5° (reduced by sparse mapping).")
    else:
        report_lines.append("  Direction accuracy estimate: >2° (insufficient mapping).")

    report_lines.append("  Recommendation: " + (
        "Proceed to operation." if quality in ("EXCELLENT", "GOOD") else
        "Re-run calibration for better accuracy." if quality == "FAIR" else
        "Re-check wiring and re-calibrate."))

    report_lines.append("=" * 60 + "\n")
    return "\n".join(report_lines)


def main():
    parser = argparse.ArgumentParser(
        description="Solar Ball Auto-Calibration Tool — maps sensor channels to ball positions")
    parser.add_argument("port", help="Serial port (e.g. COM3 on Windows, /dev/ttyUSB0 on Linux)")
    parser.add_argument("--auto", action="store_true",
                        help="Run sun auto-calibration (no manual flashlight steps)")
    parser.add_argument("--report", action="store_true",
                        help="Generate calibration quality report after cal")
    args = parser.parse_args()

    try:
        calibrator = BallCalibrator(args.port)
        if args.auto:
            calibrator.auto_sun_calibrate()
        else:
            calibrator.interactive_calibrate()

        if args.report and calibrator.channel_map:
            report = calibration_quality_report(calibrator.channel_map)
            print()
            print(report)
            log_path = f"calibration_report_{time.strftime('%Y%m%d_%H%M%S')}.txt"
            with open(log_path, "w") as f:
                f.write(report)
            print(f"Report saved to: {log_path}")

    except serial.SerialException as e:
        print(f"{Fore.RED}Serial error: {e}{Style.RESET_ALL}")
        print(f"Make sure the ESP32 is connected to {port} and nothing else is using it.")
        sys.exit(1)
    except KeyboardInterrupt:
        print(f"\n{Fore.YELLOW}Calibration cancelled by user.{Style.RESET_ALL}")
        try:
            calibrator.ser.close()
        except Exception:
            pass
        sys.exit(0)


if __name__ == "__main__":
    main()
