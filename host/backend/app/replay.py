"""Explicit, deterministic demo source; all events carry mode=REPLAY."""
from __future__ import annotations

import asyncio
import math
from typing import Any, Callable


class ReplayEngine:
    def __init__(self, publish: Callable[[dict[str, Any]], None]):
        self.publish = publish
        self.appliances = [False] * 4
        self.fault = 0
        self.feedback_mode = 2
        self.tick = 0
        self.running = True

    async def run(self) -> None:
        while self.running:
            self.tick += 1
            ts = self.tick * 50
            t = self.tick * 0.05
            loads = [1000, 75, 65, 250]
            p = sum(loads[i] for i, on in enumerate(self.appliances) if on)
            v = 230.0 + (35 * math.sin(t / 6) if self.fault == 1 else 0)
            i_rms = p / max(v, 1)
            phase = 2 * math.pi * 50 * (self.tick * 64 / 3200)
            wave = [round(3253 * math.sin(phase + 2 * math.pi * n / 64)) for n in range(64)]
            self.publish({"mode": "REPLAY", "kind": "telemetry", "data": {
                "ts_ms": ts, "v_rms": v, "i_rms": i_rms, "p_w": p, "q1_var": 0.22 * p,
                "s_va": math.hypot(p, .22*p), "pf": .977, "freq_hz": 50.0,
                "thd_i_pct": 12.0 if self.appliances[2] else 1.2, "thd_v_pct": 1.8,
                "energy_wh": p * t / 3600, "thermal_pct": min(100, i_rms*i_rms/64*100),
                "harm_i_rms": [i_rms, i_rms*.1, i_rms*.04, i_rms*.02, i_rms*.01]}})
            relay = 2 if self.fault == 0 else 6
            load_v = 12.0 if self.fault == 0 else 0.0
            self.publish({"mode": "REPLAY", "kind": "protection", "data": {
                "ts_ms": ts, "relay_state": relay, "active_fault": self.fault,
                "coil_cmd": int(self.fault == 0), "aux_state": 0, "feedback_mode": self.feedback_mode,
                "verify_samples": 0, "v_src_v": 12.0, "v_load_v": load_v, "trip_latency_us": 0}})
            self.publish({"mode": "REPLAY", "kind": "waveform", "data": {"ts_ms": ts,
                "v_samples": wave, "i_samples": [round(100*math.sin(phase + 2*math.pi*n/64)) for n in range(64)]}})
            self.publish({"mode": "REPLAY", "kind": "nilm", "data": {"ts_ms": ts,
                "active_mask": sum((1 << n) for n, on in enumerate(self.appliances) if on),
                "est_power_w": [loads[n] if on else 0 for n, on in enumerate(self.appliances)],
                "confidence": [.98 if on else .99 for on in self.appliances], "unknown_power_w": 0.0,
                "last_mahalanobis": 1.1, "inference_us": 74}})
            if self.tick % 20 == 0:
                self.publish({"mode": "REPLAY", "kind": "health", "data": {"ts_ms": ts,
                    "dsp_exec_us": 1900, "max_dsp_us": 2400, "dropped_frames": 0,
                    "uart_crc_err": 0, "nvs_records": 2, "free_stack_min": 860}})
            await asyncio.sleep(.05)

    def command(self, kind: str, payload: dict[str, Any]) -> None:
        if kind == "appliance":
            self.appliances[payload["appliance_id"]] = payload["target_state"]
        elif kind == "fault":
            self.fault = payload["fault_scenario"]
        elif kind == "reset-relay":
            self.fault = 0
        elif kind == "feedback-mode":
            self.feedback_mode = payload["feedback_mode"]
