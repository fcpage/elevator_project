from __future__ import annotations

from dataclasses import dataclass
from typing import Callable


@dataclass(frozen=True)
class CanFrame:
    can_id: int
    data: list[int]
    source: str = ""
    timestamp_ms: int = 0

    def __post_init__(self) -> None:
        if self.can_id < 0 or self.can_id > 0x7FF:
            raise ValueError("standard CAN id must be between 0x000 and 0x7FF")
        if len(self.data) > 8:
            raise ValueError("CAN data payload cannot exceed 8 bytes")
        for byte in self.data:
            if byte < 0 or byte > 0xFF:
                raise ValueError("CAN data bytes must be between 0 and 255")


class CanBus:
    def __init__(self) -> None:
        self._subscribers: dict[int | None, list[Callable[[CanFrame], None]]] = {}
        self.trace: list[CanFrame] = []

    def subscribe(self, can_id: int | None, callback: Callable[[CanFrame], None]) -> None:
        self._subscribers.setdefault(can_id, []).append(callback)

    def send(self, frame: CanFrame) -> None:
        self.trace.append(frame)
        callbacks = self._subscribers.get(frame.can_id, []) + self._subscribers.get(None, [])
        for callback in callbacks:
            callback(frame)
