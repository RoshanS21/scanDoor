import asyncio
import sys
import os

# Ensure local src is importable when running from repo root
ROOT = os.path.dirname(os.path.dirname(__file__))
SRC = os.path.join(ROOT, "src")
if SRC not in sys.path:
    sys.path.insert(0, SRC)

from doorscan.pi_gpio import WiegandConfig, PiWiegandReader, PiDoorSensor, PiLockController
from doorscan.devices import EventBus
from doorscan.worker import DoorWorker

async def main():
    bus = EventBus()
    cfg = WiegandConfig(d0_pin=17, d1_pin=27, reader_id="door-1-reader")
    reader = PiWiegandReader(cfg, bus)
    sensor = PiDoorSensor(pin=22, door_id="door-1", bus=bus)
    lock = PiLockController(pin=23, door_id="door-1", bus=bus, active_high=True)
    worker = DoorWorker("door-1", bus, lock, allow_list={12345}, unlock_duration_ms=3000)

    # Start worker and hardware (start methods are guarded and will warn if backends missing)
    await worker.start()
    try:
        reader.start()
    except Exception as e:
        print(f"Warning: reader.start() failed: {e}")
    try:
        sensor.start()
    except Exception as e:
        print(f"Warning: sensor.start() failed: {e}")
    try:
        lock.start()
    except Exception as e:
        print(f"Warning: lock.start() failed: {e}")

    print("scanDoor running. Present a card to the reader or Ctrl-C to quit.")
    try:
        while True:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        print("Exiting")

if __name__ == '__main__':
    asyncio.run(main())
