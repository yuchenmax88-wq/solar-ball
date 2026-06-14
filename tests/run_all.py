#!/usr/bin/env python3
"""Run all Solar Ball unit tests. Exit code 0 = all passed."""
import subprocess
import sys
import os

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
tests = [
    "test_direction.py",
    "test_mqtt_protocol.py",
    "test_power.py",
    "test_calibration.py",
    "test_weather.py",
    "test_saturation.py",
    "test_remote_diag.py",
    "test_autocal_validation.py",
    "test_autocal_e2e.py",
    "integration/test_integration.py",
]

all_ok = True
for test in tests:
    path = os.path.join(TESTS_DIR, test)
    print(f"\n{'='*60}")
    print(f"Running {test}...")
    print(f"{'='*60}")
    result = subprocess.run([sys.executable, path], capture_output=False)
    if result.returncode != 0:
        all_ok = False
        print(f"  FAILED: {test}")
    else:
        print(f"  PASSED: {test}")

print(f"\n{'='*60}")
if all_ok:
    print("All tests passed!")
else:
    print("Some tests FAILED!")
print(f"{'='*60}")
sys.exit(0 if all_ok else 1)
