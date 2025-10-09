import asyncio
import pytest
from src.doorscan.devices import EventBus, SimWiegandReader, SimDoorSensor, SimLockController
from src.doorscan.worker import DoorWorker

@pytest.mark.asyncio
async def test_allow_card_triggers_unlock():
    bus = EventBus()
    reader = SimWiegandReader("reader-1", bus)
    sensor = SimDoorSensor("door-1", bus)
    lock = SimLockController("door-1", bus)
    worker = DoorWorker("door-1", bus, lock, allow_list={42}, unlock_duration_ms=100)
    await worker.start()

    # capture lock responses
    responses = []
    async def on_resp(resp):
        responses.append(resp)
    bus.subscribe("lock_response", on_resp)

    await reader.emit_card(42)

    # give some time for async flow
    await asyncio.sleep(0.5)
    assert any(getattr(r, 'status', None) == 'unlocked' for r in responses)
