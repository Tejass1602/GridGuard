"""Reconnectable pyserial CDC bridge with a bounded fan-out queue."""
from __future__ import annotations

import asyncio
import os
import queue
import struct
import threading
from typing import Any

from .protocol import ProtocolError, decode_frame, decode_message, encode_frame
from .replay import ReplayEngine


class SerialBridge:
    def __init__(self, replay: bool = False, port: str | None = None):
        self.replay_mode = replay
        self.port_override = port or os.getenv("GRIDGUARD_PORT")
        self.serial = None
        self.seq = 0
        self.events: list[dict[str, Any]] = []
        self.fault_records: list[dict[str, Any]] = []
        self.subscribers: set[asyncio.Queue] = set()
        self.replay = ReplayEngine(self.publish) if replay else None
        self.task: asyncio.Task | None = None
        self._serial_lock = threading.Lock()

    def publish(self, event: dict[str, Any]) -> None:
        self.events.append(event)
        self.events = self.events[-512:]
        if event.get("kind") == "fault_record":
            self.fault_records.append(event)
            self.fault_records = self.fault_records[-64:]
        for q in tuple(self.subscribers):
            try:
                q.put_nowait(event)
            except asyncio.QueueFull:
                try:
                    q.get_nowait()
                    q.put_nowait(event)
                except (asyncio.QueueEmpty, asyncio.QueueFull):
                    continue

    async def run(self) -> None:
        if self.replay:
            await self.replay.run()
            return
        while True:
            if self.serial is None:
                self.serial = await asyncio.to_thread(self._open_port)
                if self.serial is None:
                    self.publish({"mode": "LIVE", "kind": "status", "data": {"connected": False, "detail": "Waiting for Pico CDC"}})
                    await asyncio.sleep(2)
                    continue
                self.publish({"mode": "LIVE", "kind": "status", "data": {"connected": True, "port": self.serial.port}})
            try:
                wire = await asyncio.to_thread(self.serial.read_until, b"\x00")
                if not wire:
                    continue
                try:
                    frame = decode_frame(wire)
                    data = decode_message(frame)
                    kind = data.pop("kind")
                    self.publish({"mode": "LIVE", "kind": kind, "data": data})
                except ProtocolError:
                    continue
            except (OSError, AttributeError):
                await asyncio.to_thread(self._close_port)
                self.publish({"mode": "LIVE", "kind": "status", "data": {"connected": False, "detail": "CDC disconnected"}})

    def _open_port(self):
        try:
            import serial
            from serial.tools import list_ports
        except ImportError:
            return None
        candidates = list_ports.comports()
        if self.port_override:
            candidates.sort(key=lambda p: p.device != self.port_override)
            if candidates and candidates[0].device == self.port_override:
                selected = candidates[0].device
            else:
                selected = self.port_override
        else:
            matched = [p for p in candidates if p.vid == 0x2E8A or "pico" in (p.description or "").lower() or "zephyr" in (p.description or "").lower()]
            if not matched:
                return None
            selected = matched[0].device
        try:
            return serial.Serial(selected, 115200, timeout=.2, write_timeout=.5)
        except (OSError, serial.SerialException):
            return None

    def _close_port(self):
        if self.serial is not None:
            try:
                self.serial.close()
            finally:
                self.serial = None

    async def command(self, kind: str, payload: dict[str, Any]) -> bool:
        if self.replay:
            self.replay.command(kind, payload)
            return True
        specs = {
            "appliance": (0x10, struct.pack("<BB", payload["appliance_id"], int(payload["target_state"]))),
            "fault": (0x11, struct.pack("<Bf", payload["fault_scenario"], payload["magnitude"])),
            "reset-relay": (0x12, struct.pack("<B", int(payload["force_clear_latch"]))),
            "feedback-mode": (0x13, struct.pack("<B", payload["feedback_mode"])),
            "read-faults": (0x14, struct.pack("<HH", payload["start_index"], payload["count"])),
        }
        if self.serial is None:
            return False
        msg_type, body = specs[kind]
        wire = encode_frame(msg_type, self.seq, body)
        self.seq = (self.seq + 1) & 0xFFFF
        try:
            with self._serial_lock:
                await asyncio.to_thread(self.serial.write, wire)
            return True
        except OSError:
            await asyncio.to_thread(self._close_port)
            return False

    def history(self, kind: str | None = None) -> list[dict[str, Any]]:
        if kind == "fault_record":
            return [event["data"] | {"mode": event["mode"]} for event in self.fault_records]
        return [event for event in self.events if event.get("kind") == kind] if kind else self.events
