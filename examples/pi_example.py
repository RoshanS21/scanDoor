"""Example wiring for Raspberry Pi 5 usage.

Wiring assumptions (BCM numbering):
- Wiegand D0 -> GPIO 17
- Wiegand D1 -> GPIO 27
- Door sensor (NC contact) -> GPIO 22
- Lock control (relay input) -> GPIO 23

Hardware safety notes:
- Use opto-isolators on reader lines if wiring runs are long.
- Use a proper relay/driver and separate power supply for lock.
- Ensure common ground between Pi and reader power where required.

Run on Pi with pigpiod started (sudo systemctl enable --now pigpiod)
"""
import asyncio
from src.doorscan.pi_gpio import WiegandConfig, PiWiegandReader, PiDoorSensor, PiLockController
from src.doorscan.devices import EventBus
from src.doorscan.worker import DoorWorker

async def main():
    bus = EventBus()
    cfg = WiegandConfig(d0_pin=17, d1_pin=27, reader_id="door-1-reader")
    reader = PiWiegandReader(cfg, bus)
    sensor = PiDoorSensor(pin=22, door_id="door-1", bus=bus)
    lock = PiLockController(pin=23, door_id="door-1", bus=bus, active_high=True)
    worker = DoorWorker("door-1", bus, lock, allow_list={12345}, unlock_duration_ms=3000)

    # start hardware
    reader.start()
    sensor.start()
    lock.start()
    await worker.start()

    print("Pi example running. Present card to reader or press Ctrl-C to quit.")
    try:
        while True:
            await asyncio.sleep(1)
    finally:
        reader.stop()

if __name__ == '__main__':
    asyncio.run(main())
