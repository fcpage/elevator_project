from __future__ import annotations

from typing import Protocol

from sim.can import CanBus, CanFrame
from sim.protocol import SC_CAN_ID
from sim.supervisor_process import SupervisorEvent


class SupervisorProcess(Protocol):
    def start(self) -> None:
        ...

    def stop(self) -> None:
        ...

    def send_can_rx(self, frame: CanFrame) -> SupervisorEvent | None:
        ...


class ProcessSupervisorNode:
    def __init__(self, bus: CanBus, process: SupervisorProcess) -> None:
        self.bus = bus
        self.process = process
        self.bus.subscribe(None, self._on_can_frame)

    def start(self) -> None:
        self.process.start()

    def stop(self) -> None:
        self.process.stop()

    def _on_can_frame(self, frame: CanFrame) -> None:
        if frame.can_id == SC_CAN_ID and frame.source == "supervisor_process":
            return
        event = self.process.send_can_rx(frame)
        if event is None or event.event_type != "can_tx" or event.can_id is None:
            return
        self.bus.send(CanFrame(can_id=event.can_id, data=event.data or [], source="supervisor_process"))
