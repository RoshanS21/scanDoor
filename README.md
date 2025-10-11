# scanDoor — Raspberry Pi Wiegand door controller (prototype)

Minimal, hardware-first prototype that reads Wiegand readers, monitors a door sensor, and controls a door lock.

Quick overview
--------------
- The Pi captures Wiegand card reads and publishes `CardEvent` to an internal EventBus.
- A DoorWorker evaluates the event (simple allow-list by default) and issues lock commands.
- The lock controller actuates the lock and the door sensor reports open/closed state.

Prerequisites (Pi 5 recommended)
--------------------------------
- Raspberry Pi OS (or similar) on Raspberry Pi 5.
- A Wiegand reader, door sensor (reed/magnet), and a lock driver (relay/MOSFET) with separate power supply.

System packages to install (Pi 5)
---------------------------------
Install gpiozero (uses gpiod/lgpio backends on Pi 5) and libgpiod:

```bash
sudo apt update
sudo apt install -y python3-gpiozero libgpiod2 python3-rpi.gpio
```

Optional: pigpio daemon for low-latency capture (may require building from source on Pi 5):

```bash
# apt install (may be outdated)
sudo apt install -y pigpio python3-pigpio
# or build pigpio from https://github.com/joan2937/pigpio if needed
```

Python dependencies
-------------------
Create a virtualenv and install Python deps from `requirements.txt`:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Wiring quick reference
----------------------
Primary signals (BCM → physical header):

- Wiegand D0 -> BCM 17  -> physical pin 11
- Wiegand D1 -> BCM 27  -> physical pin 13
- Door sensor  -> BCM 22 -> physical pin 15
- Lock control  -> BCM 23 -> physical pin 16

Important:
- Physical pin 17 is a 3.3V power supply. Do NOT confuse it with BCM 17 (GPIO input). Always double-check physical pins when wiring.
- Use opto-isolation or level shifting if the reader outputs are not 3.3V TTL.
- Locks must use a separate power supply and a proper driver (relay/MOSFET). Do not power locks from the Pi.

Pin callouts for this project:

- BCM17 (physical 11) — Wiegand D0
- BCM27 (physical 13) — Wiegand D1
- BCM22 (physical 15) — Door sensor input (NC preferred)
- BCM23 (physical 16) — Lock control (relay input)

Always confirm the physical pin numbers on your Pi model pinout before connecting.

Run the program
---------------
From the repository root:

```bash
source .venv/bin/activate
python3 examples/pi_example.py
```

The example uses the pins above by default. Edit `examples/pi_example.py` to change pins or allowed cards.

Troubleshooting
---------------
- If Wiegand reads are missed, ensure `gpiozero` is installed and the wiring is clean. For high reliability use pigpio or an MCU at the door.
- If `pigpiod` fails to start on Pi 5, build the latest pigpio from source (see project docs) or rely on `gpiozero`/gpiod backends.

Next steps
----------
- Add a DeviceManager for multiple doors.
- Implement a robust Wiegand decoder (26/34-bit) with parity checks.
- Add persistence, admin API, and secure OTA/firmware patterns.
