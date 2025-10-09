from __future__ import annotations
import asyncio
from dataclasses import dataclass
from typing import Callable, Dict, Optional, Any
import uuid
import time

Timestamp = float

@dataclass
class CardEvent:
    reader_id: str
    raw_bits: str
    format: str
    card_number: Optional[int]
    parity_ok: bool
    timestamp: Timestamp
    event_id: str = None

    def __post_init__(self):
        if self.event_id is None:
            self.event_id = str(uuid.uuid4())

@dataclass
class DoorSensorEvent:
    door_id: str
    state: str  # "open" | "closed"
    timestamp: Timestamp

@dataclass
class LockCommand:
    request_id: str
    door_id: str
    action: str  # "pulse" | "lock" | "unlock"
    duration_ms: Optional[int] = None
    timestamp: Timestamp = None

@dataclass
class LockResponse:
    door_id: str
    request_id: str
    status: str  # "locked"|"unlocked"|"jam"|"error"
    timestamp: Timestamp

class EventBus:
    """Simple async pub/sub event bus keyed by event name."""
    def __init__(self):
        self._subs: Dict[str, list[Callable[[Any], None]]] = {}

    def subscribe(self, event_name: str, cb: Callable[[Any], None]):
        self._subs.setdefault(event_name, []).append(cb)

    def unsubscribe(self, event_name: str, cb: Callable[[Any], None]):
        lst = self._subs.get(event_name)
        if lst and cb in lst:
            lst.remove(cb)

    async def publish(self, event_name: str, payload: Any):
        # dispatch to subscribers concurrently
        for cb in list(self._subs.get(event_name, [])):
            # run callback in its own task so a slow handler doesn't block
            asyncio.create_task(cb(payload))

# Simulated devices

class SimWiegandReader:
    def __init__(self, reader_id: str, bus: EventBus):
        self.reader_id = reader_id
        self.bus = bus

    async def emit_card(self, card_number: int, format: str = "26bit", parity_ok: bool = True):
        raw = bin(card_number)[2:]
        event = CardEvent(
            reader_id=self.reader_id,
            raw_bits=raw,
            format=format,
            card_number=card_number,
            parity_ok=parity_ok,
            timestamp=time.time(),
        )
        await self.bus.publish("card_read", event)

class SimDoorSensor:
    def __init__(self, door_id: str, bus: EventBus):
        self.door_id = door_id
        self.bus = bus
        self.state = "closed"

    async def set_state(self, state: str):
        self.state = state
        evt = DoorSensorEvent(door_id=self.door_id, state=state, timestamp=time.time())
        await self.bus.publish("door_state", evt)

class SimLockController:
    def __init__(self, door_id: str, bus: EventBus, default_pulse_ms: int = 5000):
        self.door_id = door_id
        self.bus = bus
        self.default_pulse_ms = default_pulse_ms

    async def send_command(self, cmd: LockCommand) -> LockResponse:
        # Simulate actuation delay
        duration = cmd.duration_ms or self.default_pulse_ms
        # For pulse, we unlock briefly then relock
        if cmd.action == "pulse":
            # publish unlocked
            resp_unlocked = LockResponse(door_id=self.door_id, request_id=cmd.request_id, status="unlocked", timestamp=time.time())
            await self.bus.publish("lock_response", resp_unlocked)
            # wait duration in background then publish locked
            async def relock():
                await asyncio.sleep(duration / 1000)
                resp_locked = LockResponse(door_id=self.door_id, request_id=cmd.request_id, status="locked", timestamp=time.time())
                await self.bus.publish("lock_response", resp_locked)
            asyncio.create_task(relock())
            return resp_unlocked
        elif cmd.action in ("lock", "unlock"):
            status = "locked" if cmd.action == "lock" else "unlocked"
            resp = LockResponse(door_id=self.door_id, request_id=cmd.request_id, status=status, timestamp=time.time())
            await self.bus.publish("lock_response", resp)
            return resp
        else:
            resp = LockResponse(door_id=self.door_id, request_id=cmd.request_id, status="error", timestamp=time.time())
            await self.bus.publish("lock_response", resp)
            return resp
