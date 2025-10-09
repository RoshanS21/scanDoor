# scanDoor Prototype

Simple todo list

- [ ] Implement HAL interfaces for Wiegand readers, door sensors, locks
- [ ] Implement EventBus and DoorWorker
- [ ] Provide simulated hardware for local testing
- [ ] Add persistence and admin API
- [ ] Replace simulators with real GPIO/MQTT drivers

Quick start (Python 3.11+):

1. Create a virtualenv and install dev deps:

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

2. Run the demo:

```bash
python -m src.doorscan.main
```

3. Run tests:

```bash
pytest -q
```

This repo contains a minimal async prototype demonstrating a Wiegand card event -> policy decision -> lock command -> sensor verification flow. It uses simulators so you can run without hardware.

Raspberry Pi connection guide
-----------------------------

Hardware notes (BCM numbering used in examples):

- Wiegand reader pins: typically D0, D1, VCC, GND. Many readers use 12V for VCC; check your reader's datasheet.
- Do NOT connect 12V directly to the Pi GPIO. Use opto-isolation or a level shifter if the reader output is not 3.3V tolerant.
- Suggested wiring for a single reader with opto-isolator (safe):
	- Reader D0 -> opto-isolator input -> opto output -> Pi GPIO (e.g., BCM 17)
	- Reader D1 -> opto-isolator input -> opto output -> Pi GPIO (e.g., BCM 27)
	- Reader GND -> common reference or opto-isolator ground as required (follow opto wiring)
	- Reader VCC -> reader power supply (often 12V) separate from Pi

Simple direct wiring (only if your reader's D0/D1 are 3.3V TTL outputs and rated for Pi use):

- Reader D0 -> Pi GPIO 17 (BCM)
- Reader D1 -> Pi GPIO 27 (BCM)
- Reader GND -> Pi GND
- Reader VCC -> Reader power supply (do not power the reader from Pi 3.3V unless explicitly supported)

Lock and sensor wiring hints:

- Lock (electric strike or maglock) should have its own power supply (12V/24V). Use a relay or MOSFET with proper flyback protection and opto-isolation where possible.
- Lock control -> Pi GPIO through a relay driver (e.g., transistor + diode) or an opto-isolated relay board. Example: Pi GPIO 23 -> relay module -> lock power.
- Door sensor (magnetic/reed): wire to a Pi GPIO configured with internal pull-up/pull-down. Use normally-closed (NC) contacts for better tamper detection.

Software: running on Raspberry Pi
--------------------------------

1. Install pigpio and start the daemon (required for reliable Wiegand timing):

```bash
sudo apt update
sudo apt install pigpio
sudo systemctl enable --now pigpiod
```

2. Create and activate a virtualenv (recommended):

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

3. Run the Pi example which uses BCM pins 17/27 for Wiegand, 22 for door sensor, 23 for lock (edit `examples/pi_example.py` if you use different pins):

```bash
python3 examples/pi_example.py
```

4. Run the simulator/demo locally (no hardware) to exercise the logic:

```bash
python3 -m src.doorscan.main
```

5. Run tests:

```bash
python3 -m pytest -q
```

Safety and troubleshooting
--------------------------

- If you see missed or garbled card reads, ensure `pigpiod` is running and that your wiring uses clean signals (use short, shielded wires when possible).
- If the Pi misses pulses under load, consider placing a per-door MCU (ESP32/Arduino) to capture Wiegand at the door and forward framed messages to the Pi over RS485/MQTT.
- Log electrical faults and check grounds. Use fuses on lock power circuits.
