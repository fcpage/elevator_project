from __future__ import annotations

from collections import deque

from sim.can import CanBus, CanFrame
from sim.protocol import (
    CC_CAN_ID,
    EC_CAN_ID,
    FLOOR_CAN_IDS,
    encode_supervisor_command,
    floor_from_payload,
    payload_has_status_or_enable,
)


class InProcessSupervisor:
    """Tiny reference supervisor for simulator self-tests.

    Real tests should use JsonLineSupervisorProcess so the C++ controller is the
    code under test. This class keeps the Python harness testable by itself.
    """

    def __init__(self) -> None:
        self._bus: CanBus | None = None
        self._pending: deque[int] = deque()
        self._busy = False

    def attach(self, bus: CanBus) -> None:
        self._bus = bus
        bus.subscribe(None, self._on_can_frame)

    def request_floor_from_web(self, floor: int) -> None:
        self._pending.append(floor)
        self._dispatch_next()

    def _on_can_frame(self, frame: CanFrame) -> None:
        if frame.can_id in FLOOR_CAN_IDS.values() or frame.can_id == CC_CAN_ID:
            if frame.data:
                if frame.can_id == CC_CAN_ID:
                    self._pending.append(floor_from_payload(frame.data[0]))
                else:
                    self._pending.append(_floor_from_floor_controller_id(frame.can_id))
                self._dispatch_next()
        elif frame.can_id == EC_CAN_ID and len(frame.data) == 1 and not payload_has_status_or_enable(frame.data[0]):
            self._busy = False
            self._dispatch_next()

    def _dispatch_next(self) -> None:
        if self._bus is None or self._busy or not self._pending:
            return
        floor = self._pending.popleft()
        self._busy = True
        self._bus.send(encode_supervisor_command(floor, enable=True))


def _floor_from_floor_controller_id(can_id: int) -> int:
    for floor, floor_can_id in FLOOR_CAN_IDS.items():
        if can_id == floor_can_id:
            return floor
    return 0
