#!/usr/bin/env python3
"""Check which GPIO backends are visible to the current Python environment.

Run this inside your project's venv to see which modules gpiozero can use.

Usage:
  # activate venv then
  python3 scripts/check_gpio_backends.py

This prints import results for: gpiozero, lgpio, gpiod, pigpio, RPi.GPIO and shows
Device.pin_factory. It will also print recommended next steps based on findings.
"""

import sys
import importlib
import platform
import os

results = {}

print("Python executable:", sys.executable)
print("Python version:", platform.python_version())
print("Current user:", os.getenv('USER') or os.getlogin())
print("GPIOZERO_PIN_FACTORY env:", os.getenv('GPIOZERO_PIN_FACTORY'))
print()

modules = ["gpiozero", "lgpio", "gpiod", "pigpio", "RPi.GPIO"]
for m in modules:
    try:
        mod = importlib.import_module(m)
        ver = getattr(mod, "__version__", None)
        results[m] = (True, ver or "available")
    except Exception as e:
        results[m] = (False, str(e))

for m, (ok, info) in results.items():
    print(f"{m}:", "OK," if ok else "FAIL,", info)

# gpiozero pin factory check
try:
    from gpiozero import Device
    pf = Device.pin_factory
    print("\ngpiozero Device.pin_factory:", pf)
except Exception as e:
    print("\nCould not import gpiozero or read Device.pin_factory:", e)

print("\nRecommendations:")
if not results.get("gpiozero", (False,))[0]:
    print(" - gpiozero is not importable in this venv. Install it with: pip install gpiozero")
else:
    if results.get("lgpio", (False,))[0]:
        print(" - lgpio is importable. Ensure you use the same python interpreter (sys.executable) when running your app.")
    if results.get("gpiod", (False,))[0]:
        print(" - gpiod python bindings are available. Set GPIOZERO_PIN_FACTORY=gpiod if you prefer libgpiod backend.")
    if not (results.get("lgpio", (False,))[0] or results.get("gpiod", (False,))[0]):
        print(" - No lgpio/gpiod binding visible to this venv. Install system packages or a matching python package:")
        print("     sudo apt install python3-lgpio libgpiod2 python3-gpiozero")
        print("   Or, if installed system-wide, make sure your venv can see system-site-packages or install via pip if available.")
    if not results.get("pigpio", (False,))[0]:
        print(" - pigpio python module not importable. If you want to use pigpio, ensure pigpio library and pigpiod are installed and pigpio python package is present.")

print("\nIf you just want a fast check, run this script with sudo -E to eliminate permission issues, but prefer fixing group membership instead.")

sys.exit(0)
