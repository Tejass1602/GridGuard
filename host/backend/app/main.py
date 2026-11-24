from __future__ import annotations

import asyncio
import csv
import io
import os
import time
from contextlib import asynccontextmanager, suppress

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import StreamingResponse

from .schemas import ApplianceCommand, FaultCommand, FeedbackModeCommand, ResetCommand
from .serial_bridge import SerialBridge

REPLAY = os.getenv("GRIDGUARD_REPLAY", "0").lower() in {"1", "true", "yes"}
bridge = SerialBridge(replay=REPLAY)
last_fault_query = 0.0


@asynccontextmanager
async def lifespan(_app: FastAPI):
    bridge.task = asyncio.create_task(bridge.run(), name="gridguard-serial-or-replay")
    try:
        yield
    finally:
        if bridge.replay:
            bridge.replay.running = False
        bridge.task.cancel()
        with suppress(asyncio.CancelledError):
            await bridge.task
        bridge._close_port()


app = FastAPI(title="GridGuard API", version="1.0", lifespan=lifespan)


@app.get("/api/status")
async def status():
    return {"mode": "REPLAY" if bridge.replay_mode else "LIVE", "connected": bridge.replay_mode or bridge.serial is not None}


@app.websocket("/ws/live")
async def live(websocket: WebSocket):
    await websocket.accept()
    q: asyncio.Queue = asyncio.Queue(maxsize=64)
    bridge.subscribers.add(q)
    try:
        await websocket.send_json({"mode": "REPLAY" if bridge.replay_mode else "LIVE", "kind": "status", "data": {"connected": bridge.replay_mode or bridge.serial is not None}})
        while True:
            await websocket.send_json(await q.get())
    except WebSocketDisconnect:
        return
    finally:
        bridge.subscribers.discard(q)


async def dispatch(kind: str, payload: dict) -> dict:
    if not await bridge.command(kind, payload):
        raise HTTPException(status_code=409, detail="Pico CDC is not connected")
    return {"accepted": True, "mode": "REPLAY" if bridge.replay_mode else "LIVE"}


@app.post("/api/cmd/appliance")
async def appliance(command: ApplianceCommand):
    return await dispatch("appliance", command.model_dump())


@app.post("/api/cmd/fault")
async def fault(command: FaultCommand):
    return await dispatch("fault", command.model_dump())


@app.post("/api/cmd/reset-relay")
async def reset_relay(command: ResetCommand):
    return await dispatch("reset-relay", command.model_dump())


@app.post("/api/cmd/feedback-mode")
async def feedback_mode(command: FeedbackModeCommand):
    return await dispatch("feedback-mode", command.model_dump())


@app.get("/api/faults")
async def faults():
    global last_fault_query
    now = time.monotonic()
    if not bridge.replay_mode and now - last_fault_query >= 2.0:
        await bridge.command("read-faults", {"start_index": 0, "count": 64})
        last_fault_query = now
    return bridge.history("fault_record")


@app.get("/api/faults/export.csv")
async def export_faults():
    rows = bridge.history("fault_record")
    stream = io.StringIO()
    fields = ["record_id", "ts_ms", "fault_code", "relay_result", "coil_cmd", "aux_state", "trigger_val", "threshold_val", "v_src_v", "v_load_v", "verify_ms", "crc32", "mode"]
    writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
    writer.writeheader()
    writer.writerows(rows)
    stream.seek(0)
    return StreamingResponse(iter([stream.getvalue()]), media_type="text/csv", headers={"Content-Disposition": "attachment; filename=gridguard_faults.csv"})


if __name__ == "__main__":
    import argparse
    import uvicorn
    parser = argparse.ArgumentParser(description="GridGuard FastAPI host")
    parser.add_argument("--replay", action="store_true", help="run the explicitly labeled replay source")
    parser.add_argument("--port", help="serial port override, such as COM5 or /dev/ttyACM0")
    args = parser.parse_args()
    if args.port:
        bridge.port_override = args.port
    if args.replay and not bridge.replay_mode:
        bridge.replay_mode = True
        from .replay import ReplayEngine
        bridge.replay = ReplayEngine(bridge.publish)
    uvicorn.run(app, host="127.0.0.1", port=8000, reload=False)
