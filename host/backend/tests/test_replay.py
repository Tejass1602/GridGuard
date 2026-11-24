import asyncio

from app.replay import ReplayEngine


def test_replay_emits_labeled_schema_and_accepts_scenario_commands():
    async def exercise():
        events = []
        engine = ReplayEngine(events.append)
        task = asyncio.create_task(engine.run())
        await asyncio.sleep(.06)
        engine.running = False
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)
        return events, engine

    events, engine = asyncio.run(exercise())
    telemetry = next(event for event in events if event["kind"] == "telemetry")
    assert telemetry["mode"] == "REPLAY"
    assert {"v_rms", "i_rms", "p_w", "freq_hz"} <= telemetry["data"].keys()
    engine.command("appliance", {"appliance_id": 0, "target_state": True})
    engine.command("fault", {"fault_scenario": 3})
    assert engine.appliances[0]
    assert engine.fault == 3
