from typing import Literal
from pydantic import BaseModel, Field


class ApplianceCommand(BaseModel):
    appliance_id: int = Field(ge=0, le=3)
    target_state: bool


class FaultCommand(BaseModel):
    fault_scenario: int = Field(ge=0, le=7)
    magnitude: float = Field(default=0.0, ge=0, le=1000)


class ResetCommand(BaseModel):
    force_clear_latch: bool = False


class FeedbackModeCommand(BaseModel):
    feedback_mode: int = Field(ge=0, le=4)


class LiveEnvelope(BaseModel):
    mode: Literal["LIVE", "REPLAY"]
    kind: str
    data: dict
