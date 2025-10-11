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

