from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


NORMAL_ACTIONS = {
    "gui_request",
    "floor_call",
    "car_request",
    "wait_for_can",
    "wait_for_database",
    "wait",
}
FAULT_ACTIONS = {
    "suppress_heartbeat",
    "delay_arrival",
    "database_outage",
    "drop_next_frame",
}
SUPPORTED_ACTIONS = NORMAL_ACTIONS | FAULT_ACTIONS


def _normalize_legacy_step(raw: dict[str, Any]) -> dict[str, Any]:
    """Accept the pre-refit basic scenario format without weakening v1 files."""
    if "action" in raw:
        return raw
    legacy_type = raw.get("type")
    if legacy_type == "floor_call":
        return {"action": "floor_call", "floor": raw.get("floor")}
    if legacy_type == "advance":
        return {"action": "wait", "ms": raw.get("ms")}
    if legacy_type == "expect_floor":
        return {
            "action": "wait_for_can",
            "id": "0x101",
            "data_byte": raw.get("floor"),
            "timeout_ms": raw.get("timeout_ms", 10_000),
        }
    return raw


@dataclass(frozen=True)
class ScenarioStep:
    action: str
    arguments: dict[str, Any]


@dataclass(frozen=True)
class Scenario:
    version: int
    name: str
    description: str
    repeat: bool
    journeys: tuple[tuple[ScenarioStep, ...], ...]

    @property
    def steps(self) -> tuple[ScenarioStep, ...]:
        """Flattened compatibility view for existing scenario consumers."""
        return tuple(step for journey in self.journeys for step in journey)


def load_scenario(path: Path) -> Scenario:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("version") != 1:
        raise ValueError("scenario version must be 1")
    name = payload.get("name")
    if not isinstance(name, str) or not name:
        raise ValueError("scenario name must be a non-empty string")
    raw_journeys = payload.get("journeys")
    raw_steps = payload.get("steps")
    if raw_journeys is not None and raw_steps is not None:
        raise ValueError("scenario must define either journeys or steps, not both")
    if raw_journeys is None:
        raw_journeys = [raw_steps]
    if not isinstance(raw_journeys, list) or not raw_journeys:
        raise ValueError("scenario journeys must be a non-empty list")

    journeys: list[tuple[ScenarioStep, ...]] = []
    for journey_index, journey in enumerate(raw_journeys):
        if isinstance(journey, dict):
            journey = journey.get("steps")
        if not isinstance(journey, list) or not journey:
            raise ValueError(f"scenario journey {journey_index} steps must be a non-empty list")
        steps: list[ScenarioStep] = []
        for step_index, raw in enumerate(journey):
            if not isinstance(raw, dict):
                raise ValueError(
                    f"scenario journey {journey_index} step {step_index} must be an object"
                )
            raw = _normalize_legacy_step(raw)
            action = raw.get("action")
            if action not in SUPPORTED_ACTIONS:
                raise ValueError(
                    f"scenario journey {journey_index} step {step_index} "
                    f"has unsupported action: {action}"
                )
            steps.append(
                ScenarioStep(
                    action=action,
                    arguments={k: v for k, v in raw.items() if k != "action"},
                )
            )
        journeys.append(tuple(steps))

    return Scenario(
        version=1,
        name=name,
        description=str(payload.get("description", "")),
        repeat=bool(payload.get("repeat", False)),
        journeys=tuple(journeys),
    )
