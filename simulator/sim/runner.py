from __future__ import annotations

from dataclasses import dataclass, field

from sim.can import CanBus
from sim.clock import SimClock
from sim.nodes import ElevatorControllerNode, FloorControllerNode
from sim.supervisor import InProcessSupervisor
from sim.supervisor_node import ProcessSupervisorNode


@dataclass
class ScenarioResult:
    passed: bool
    final_floor: int
    events: list[str] = field(default_factory=list)


def run_builtin_scenario(name: str, supervisor: InProcessSupervisor) -> ScenarioResult:
    if name != "basic_floor_request":
        raise ValueError(f"unknown built-in scenario: {name}")

    clock = SimClock()
    bus = CanBus()
    events: list[str] = []
    elevator = ElevatorControllerNode(bus, clock, travel_ms_per_floor=1000)
    floor_3 = FloorControllerNode(bus, 3)
    supervisor.attach(bus)

    floor_3.press_call_button()
    events.append("floor_request:3")

    for _ in range(3):
        clock.advance(1000)
        elevator.update()

    if elevator.current_floor == 3:
        events.append("arrived:3")

    return ScenarioResult(
        passed=elevator.current_floor == 3,
        final_floor=elevator.current_floor,
        events=events,
    )


def run_process_scenario(name: str, process) -> ScenarioResult:
    if name != "basic_floor_request":
        raise ValueError(f"unknown process scenario: {name}")

    clock = SimClock()
    bus = CanBus()
    events: list[str] = []
    elevator = ElevatorControllerNode(bus, clock, travel_ms_per_floor=1000)
    floor_3 = FloorControllerNode(bus, 3)
    supervisor = ProcessSupervisorNode(bus, process)

    supervisor.start()
    try:
        floor_3.press_call_button()
        events.append("floor_request:3")

        for _ in range(3):
            clock.advance(1000)
            elevator.update()

        if elevator.current_floor == 3:
            events.append("arrived:3")

        return ScenarioResult(
            passed=elevator.current_floor == 3,
            final_floor=elevator.current_floor,
            events=events,
        )
    finally:
        supervisor.stop()
