import asyncio
import time
from .devices import EventBus, SimWiegandReader, SimDoorSensor, SimLockController
from .worker import DoorWorker

async def demo():
    bus = EventBus()
    # single door components
    reader = SimWiegandReader("reader-1", bus)
    sensor = SimDoorSensor("door-1", bus)
    lock = SimLockController("door-1", bus)
    # allowlist contains card 12345
    worker = DoorWorker("door-1", bus, lock, allow_list={12345}, unlock_duration_ms=3000)
    await worker.start()

    # Simulate a denied card
    print("-- emitting denied card 99999")
    await reader.emit_card(99999)
    await asyncio.sleep(0.5)

    # Simulate allowed card
    print("-- emitting allowed card 12345")
    await reader.emit_card(12345)
    # sensor reports door open after 0.2s
    async def open_after():
        await asyncio.sleep(0.2)
        await sensor.set_state("open")
        await asyncio.sleep(1.0)
        await sensor.set_state("closed")
    asyncio.create_task(open_after())

    # run for a short while to observe
    await asyncio.sleep(5)

if __name__ == "__main__":
    asyncio.run(demo())
