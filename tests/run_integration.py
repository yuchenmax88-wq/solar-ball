#!/usr/bin/env python3
"""Run all Solar Ball integration tests."""
import subprocess
import sys
import os

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
integration_dir = os.path.join(TESTS_DIR, "integration")
test_file = os.path.join(integration_dir, "test_integration.py")

print("=" * 60)
print("Solar Ball Integration Tests")
print("=" * 60)
print()

result = subprocess.run(
    [sys.executable, test_file],
    capture_output=False,
    cwd=TESTS_DIR
)

print()
print("=" * 60)
if result.returncode == 0:
    print("Integration tests passed!")
else:
    print("Integration tests FAILED!")
print("=" * 60)

sys.exit(result.returncode)
