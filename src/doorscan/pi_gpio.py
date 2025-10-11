"""Raspberry Pi GPIO drivers for Wiegand, door sensor, and lock.

This module implements a Wiegand reader using the pigpio daemon for reliable edge timing.
It falls back to RPi.GPIO if pigpio is not available.

Usage notes:
- Install pigpio on the Pi and start the daemon: `sudo apt install pigpio` and `sudo systemctl enable --now pigpiod`
- Run the python code as root or with access to gpio (use `sudo` or add the user to gpio group).
"""
from __future__ import annotations
import time
import asyncio
from dataclasses import dataclass
from typing import Callable, Optional

try:
    import pigpio
    PIGPIO_AVAILABLE = True
except Exception:
    PIGPIO_AVAILABLE = False
    pigpio = None

try:
    import RPi.GPIO as GPIO
    RPIGPIO_AVAILABLE = True
except Exception:
    RPIGPIO_AVAILABLE = False
    GPIO = None

# Prefer gpiozero (which can use gpiod/lgpio backends on newer Pi models)
try:
    from gpiozero import Button, DigitalOutputDevice
    GPIOZERO_AVAILABLE = True
except Exception:
    GPIOZERO_AVAILABLE = False
    Button = None
    DigitalOutputDevice = None

# Small helper dataclasses re-used from devices.py to avoid circular import
from .devices import CardEvent, EventBus

@dataclass
class WiegandConfig:
    d0_pin: int
    d1_pin: int
    reader_id: str
    pulse_timeout_ms: int = 25  # max time between bits before frame considered complete

class PiWiegandReader:
    """Wiegand reader using pigpio for robust edge capture.

    Emits CardEvent instances on the provided EventBus under event name "card_read".
    """
    def __init__(self, config: WiegandConfig, bus: EventBus):
        self.config = config
        self.bus = bus
        self._bits = []
        self._last_bit_ts = None
        self._running = False
        self._loop = asyncio.get_event_loop()

        # pigpio state
        self._pi = None
        self._d0_cb = None
        self._d1_cb = None

    def start(self):
        # Prefer gpiozero (works with gpiod/lgpio on Pi 5). If not available, use pigpio or RPi.GPIO with fallbacks.
        if GPIOZERO_AVAILABLE:
            try:
                self._start_gpiozero()
                return
            except Exception as e:
                print(f"Warning: gpiozero start failed: {e}")
                # continue to other backends

        if PIGPIO_AVAILABLE:
            try:
                self._start_pigpio()
                return
            except RuntimeError as e:
                print(f"Warning: pigpio daemon unavailable ({e})")

        if RPIGPIO_AVAILABLE:
            self._start_rpigpio()
            return

        raise RuntimeError("No GPIO backend available. Install gpiozero, pigpio, or run on a Raspberry Pi with RPi.GPIO.")

    def _start_gpiozero(self):
        # Use gpiozero Button to detect falling edges reliably using gpiod/lgpio backend
        self._d0_btn = Button(self.config.d0_pin, pull_up=True, bounce_time=None)
        self._d1_btn = Button(self.config.d1_pin, pull_up=True, bounce_time=None)
        self._d0_btn.when_pressed = lambda: self._record_bit(0)
        self._d1_btn.when_pressed = lambda: self._record_bit(1)
        self._running = True

    def stop(self):
        self._running = False
        if PIGPIO_AVAILABLE and self._pi:
            if self._d0_cb:
                self._d0_cb.cancel()
                self._d0_cb = None
            if self._d1_cb:
                self._d1_cb.cancel()
                self._d1_cb = None
            self._pi = None
        if RPIGPIO_AVAILABLE:
            try:
                GPIO.remove_event_detect(self.config.d0_pin)
                GPIO.remove_event_detect(self.config.d1_pin)
            except Exception:
                pass

    # pigpio implementation
    def _start_pigpio(self):
        self._pi = pigpio.pi()
        if not self._pi.connected:
            raise RuntimeError("Could not connect to pigpio daemon (is pigpiod running?)")
        self._pi.set_mode(self.config.d0_pin, pigpio.INPUT)
        self._pi.set_mode(self.config.d1_pin, pigpio.INPUT)
        self._pi.set_pull_up_down(self.config.d0_pin, pigpio.PUD_UP)
        self._pi.set_pull_up_down(self.config.d1_pin, pigpio.PUD_UP)
        # pigpio callbacks
        self._d0_cb = self._pi.callback(self.config.d0_pin, pigpio.FALLING_EDGE, self._pigpio_d0)
        self._d1_cb = self._pi.callback(self.config.d1_pin, pigpio.FALLING_EDGE, self._pigpio_d1)

    def _pigpio_d0(self, gpio, level, tick):
        # D0 = 0 bit on falling edge
        self._record_bit(0)

    def _pigpio_d1(self, gpio, level, tick):
        # D1 = 1 bit on falling edge
        self._record_bit(1)

    # RPi.GPIO fallback
    def _start_rpigpio(self):
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(self.config.d0_pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.setup(self.config.d1_pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.add_event_detect(self.config.d0_pin, GPIO.FALLING, callback=lambda ch: self._record_bit(0), bouncetime=1)
        GPIO.add_event_detect(self.config.d1_pin, GPIO.FALLING, callback=lambda ch: self._record_bit(1), bouncetime=1)

    # expose whether we're using pigpio daemon backend
    @property
    def using_pigpio(self) -> bool:
        return bool(self._pi)

    # core bit collection
    def _record_bit(self, bit: int):
        now = time.time()
        # called from pigpio callback threads or RPi.GPIO; keep logic simple and schedule onto asyncio loop
        try:
            self._loop.call_soon_threadsafe(self._record_bit_async, bit, now)
        except Exception:
            # if loop closed
            pass

    def _record_bit_async(self, bit: int, ts: float):
        # append bit
        self._bits.append(str(bit))
        self._last_bit_ts = ts
        # schedule frame completion check
        self._loop.call_later(self.config.pulse_timeout_ms/1000.0 + 0.005, self._maybe_flush)

    def _maybe_flush(self):
        # if enough time has elapsed since last bit, consider frame complete
        if not self._bits:
            return
        now = time.time()
        if self._last_bit_ts and (now - self._last_bit_ts) >= (self.config.pulse_timeout_ms/1000.0):
            raw = ''.join(self._bits)
            # naive decode: interpret raw bits as integer
            try:
                card_num = int(raw, 2)
            except Exception:
                card_num = None
            evt = CardEvent(reader_id=self.config.reader_id, raw_bits=raw, format="raw", card_number=card_num, parity_ok=True, timestamp=time.time())
            # publish asynchronously
            asyncio.create_task(self.bus.publish("card_read", evt))
            self._bits = []
            self._last_bit_ts = None


# Simple GPIO driver for door sensor and lock
class PiDoorSensor:
    def __init__(self, pin: int, door_id: str, bus: EventBus):
        self.pin = pin
        self.door_id = door_id
        self.bus = bus
        self._loop = asyncio.get_event_loop()
        self._pi = None
        self._cb = None

    def start(self):
        # Prefer gpiozero
        if GPIOZERO_AVAILABLE:
            self._btn = Button(self.pin, pull_up=True, bounce_time=0.05)
            self._btn.when_pressed = lambda: self._on_change()
            self._btn.when_released = lambda: self._on_change()
            return

        # pigpio fallback
        if PIGPIO_AVAILABLE:
            self._pi = pigpio.pi()
            if not self._pi.connected:
                raise RuntimeError("pigpio daemon not available for door sensor")
            self._pi.set_mode(self.pin, pigpio.INPUT)
            self._pi.set_pull_up_down(self.pin, pigpio.PUD_UP)
            self._cb = self._pi.callback(self.pin, pigpio.EITHER_EDGE, lambda g, l, t: self._on_change())
            return

        # last resort: RPi.GPIO
        if RPIGPIO_AVAILABLE:
            GPIO.setmode(GPIO.BCM)
            GPIO.setup(self.pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
            GPIO.add_event_detect(self.pin, GPIO.BOTH, callback=lambda ch: self._on_change(), bouncetime=50)
            return

        raise RuntimeError("No GPIO backend available for door sensor")

    def _on_change(self):
        try:
            self._loop.call_soon_threadsafe(self._publish_state)
        except Exception:
            pass

    def _publish_state(self):
        if GPIOZERO_AVAILABLE and hasattr(self, '_btn'):
            state = 'open' if self._btn.is_pressed else 'closed'
        elif RPIGPIO_AVAILABLE:
            state = 'open' if GPIO.input(self.pin) else 'closed'
        elif self._pi:
            # pigpio returns level 0/1
            level = self._pi.read(self.pin)
            state = 'open' if level else 'closed'
        else:
            state = 'unknown'
        asyncio.create_task(self.bus.publish('door_state', type('S', (), {'door_id': self.door_id, 'state': state, 'timestamp': time.time()})))

class PiLockController:
    def __init__(self, pin: int, door_id: str, bus: EventBus, active_high: bool = True):
        self.pin = pin
        self.door_id = door_id
        self.bus = bus
        self.active_high = active_high
        self._pi = None

    def start(self):
        # prefer gpiozero
        if GPIOZERO_AVAILABLE:
            self._out = DigitalOutputDevice(self.pin, active_high=self.active_high, initial_value=False)
            # ensure locked (inactive)
            self._out.off()
            return

        if RPIGPIO_AVAILABLE:
            GPIO.setmode(GPIO.BCM)
            GPIO.setup(self.pin, GPIO.OUT)
            # ensure locked initial state
            GPIO.output(self.pin, not self.active_high)
            return

        if PIGPIO_AVAILABLE:
            self._pi = pigpio.pi()
            if not self._pi.connected:
                raise RuntimeError("pigpio daemon not available for lock controller")
            self._pi.set_mode(self.pin, pigpio.OUTPUT)
            self._pi.write(self.pin, 0 if not self.active_high else 1)
            # set to locked state (inverse of active_high)
            self._pi.write(self.pin, 0 if self.active_high else 1)
            return

        raise RuntimeError("No GPIO backend available for lock controller")

    async def send_command(self, cmd):
        # blocking but short; could be moved to executor
        action = cmd.action
        if action == 'pulse':
            duration = (cmd.duration_ms or 3000) / 1000.0
            if GPIOZERO_AVAILABLE and hasattr(self, '_out'):
                self._out.on()
            elif RPIGPIO_AVAILABLE:
                GPIO.output(self.pin, self.active_high)
            elif self._pi:
                self._pi.write(self.pin, 1 if self.active_high else 0)
            else:
                # nothing to do
                pass
            # publish unlocked
            await self.bus.publish('lock_response', type('R', (), {'door_id': self.door_id, 'request_id': cmd.request_id, 'status': 'unlocked', 'timestamp': time.time()}))
            await asyncio.sleep(duration)
            if GPIOZERO_AVAILABLE and hasattr(self, '_out'):
                self._out.off()
            elif RPIGPIO_AVAILABLE:
                GPIO.output(self.pin, not self.active_high)
            elif self._pi:
                self._pi.write(self.pin, 0 if self.active_high else 1)
            else:
                pass
            await self.bus.publish('lock_response', type('R', (), {'door_id': self.door_id, 'request_id': cmd.request_id, 'status': 'locked', 'timestamp': time.time()}))
        elif action in ('lock','unlock'):
            val = True if action == 'unlock' else False
            if GPIOZERO_AVAILABLE and hasattr(self, '_out'):
                if val:
                    self._out.on()
                else:
                    self._out.off()
            elif RPIGPIO_AVAILABLE:
                GPIO.output(self.pin, self.active_high if val else not self.active_high)
            elif self._pi:
                self._pi.write(self.pin, 1 if (self.active_high if val else not self.active_high) else 0)
            else:
                pass
            status = 'unlocked' if action == 'unlock' else 'locked'
            await self.bus.publish('lock_response', type('R', (), {'door_id': self.door_id, 'request_id': cmd.request_id, 'status': status, 'timestamp': time.time()}))
        else:
            await self.bus.publish('lock_response', type('R', (), {'door_id': self.door_id, 'request_id': cmd.request_id, 'status': 'error', 'timestamp': time.time()}))
