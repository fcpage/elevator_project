from __future__ import annotations

from sim.can import CanBus, CanFrame
from sim.clock import SimClock
from sim.protocol import (
    CC_CAN_ID,
    EC_CAN_ID,
    EC_STATUS_IDLE,
    EC_STATUS_MOVING,
    FLOOR_CAN_IDS,
    MAX_FLOOR,
    MIN_FLOOR,
    SC_CAN_ID,
    floor_from_payload,
    payload_has_status_or_enable,
)


class ElevatorControllerNode:
    def __init__(
        self,
        bus: CanBus,
        clock: SimClock,
        *,
        min_floor: int = MIN_FLOOR,
        max_floor: int = MAX_FLOOR,
        travel_ms_per_floor: int = 2000,
    ) -> None:
        self.bus = bus
        self.clock = clock
        self.min_floor = min_floor
        self.max_floor = max_floor
        self.travel_ms_per_floor = travel_ms_per_floor
        self.current_floor = min_floor
        self.target_floor = min_floor
        self._move_started_ms: int | None = None
        self._move_start_floor = min_floor
        self.bus.subscribe(SC_CAN_ID, self._on_supervisor_command)

    def _on_supervisor_command(self, frame: CanFrame) -> None:
        if len(frame.data) != 1:
            return
        payload = frame.data[0]
        if not payload_has_status_or_enable(payload):
            return
        requested_floor = floor_from_payload(payload)
        if requested_floor < self.min_floor or requested_floor > self.max_floor:
            return

        self.target_floor = requested_floor
        self._move_start_floor = self.current_floor
        self._move_started_ms = self.clock.now_ms
        self._publish(EC_STATUS_MOVING)

    def update(self) -> None:
        if self._move_started_ms is None:
            return
        floors_to_travel = abs(self.target_floor - self._move_start_floor)
        travel_time = floors_to_travel * self.travel_ms_per_floor
        if self.clock.now_ms - self._move_started_ms < travel_time:
            return

        self.current_floor = self.target_floor
        self._move_started_ms = None
        self._publish(EC_STATUS_IDLE)

    def _publish(self, status: int) -> None:
        payload = status | self.current_floor
        self.bus.send(
            CanFrame(
                can_id=EC_CAN_ID,
                data=[payload],
                source="elevator_controller",
                timestamp_ms=self.clock.now_ms,
            )
        )


class FloorControllerNode:
    def __init__(self, bus: CanBus, floor: int) -> None:
        if floor not in FLOOR_CAN_IDS:
            raise ValueError("floor must be 1, 2, or 3")
        self.bus = bus
        self.floor = floor

    def press_call_button(self) -> None:
        self.bus.send(CanFrame(can_id=FLOOR_CAN_IDS[self.floor], data=[0x01], source=f"floor_{self.floor}"))


class CarControllerNode:
    def __init__(self, bus: CanBus) -> None:
        self.bus = bus

    def press_floor_button(self, floor: int) -> None:
        self.bus.send(CanFrame(can_id=CC_CAN_ID, data=[floor], source="car_controller"))
