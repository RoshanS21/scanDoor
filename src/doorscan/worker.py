import asyncio
from typing import Set
import time
import uuid
from .devices import EventBus, CardEvent, LockCommand, DoorSensorEvent

class DoorWorker:
    def __init__(self, door_id: str, bus: EventBus, lock, allow_list: Set[int] | None = None, unlock_duration_ms: int = 5000):
        self.door_id = door_id
        self.bus = bus
        self.lock = lock
        self.allow_list = allow_list or set()
        self.unlock_duration_ms = unlock_duration_ms
        self._task = None
        self._pending_lock_requests: dict[str, asyncio.Future] = {}

    async def start(self):
        self.bus.subscribe("card_read", self._on_card)
        self.bus.subscribe("lock_response", self._on_lock_response)
        self.bus.subscribe("door_state", self._on_door_state)

    async def stop(self):
        # naive unsubscribe by recreating bus subs not implemented; in prototype we leave it
        pass

    async def _on_card(self, evt: CardEvent):
        # filter by reader->door if applicable; for prototype accept any reader for this door
        print(f"[DoorWorker:{self.door_id}] Card read: {evt.card_number} (parity_ok={evt.parity_ok})")
        if not evt.parity_ok:
            print("Parity failure, ignoring")
            return
        decision = "deny"
        if evt.card_number in self.allow_list:
            decision = "allow"
        print(f"Decision: {decision}")
        if decision == "allow":
            await self._unlock_for_card(evt)

    async def _unlock_for_card(self, evt: CardEvent):
        req_id = str(uuid.uuid4())
        cmd = LockCommand(request_id=req_id, door_id=self.door_id, action="pulse", duration_ms=self.unlock_duration_ms, timestamp=time.time())
        fut = asyncio.get_event_loop().create_future()
        self._pending_lock_requests[req_id] = fut
        print(f"Sending lock command {req_id}")
        await self.lock.send_command(cmd)
        try:
            # wait for first response or timeout
            await asyncio.wait_for(fut, timeout=2.0)
            print(f"Lock command {req_id} acknowledged: {fut.result().status}")
        except asyncio.TimeoutError:
            print(f"Lock command {req_id} timed out waiting for response")
        finally:
            self._pending_lock_requests.pop(req_id, None)

    async def _on_lock_response(self, resp):
        # match by door and request id
        if getattr(resp, "door_id", None) != self.door_id:
            return
        req = getattr(resp, "request_id", None)
        if req and req in self._pending_lock_requests:
            fut = self._pending_lock_requests[req]
            if not fut.done():
                fut.set_result(resp)

    async def _on_door_state(self, evt: DoorSensorEvent):
        if evt.door_id != self.door_id:
            return
        print(f"[DoorWorker:{self.door_id}] Door state: {evt.state}")
