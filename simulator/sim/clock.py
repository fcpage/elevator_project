from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable


@dataclass
class SimClock:
    now_ms: int = 0
    _callbacks: list[Callable[[int], None]] = field(default_factory=list)

    def subscribe(self, callback: Callable[[int], None]) -> None:
        self._callbacks.append(callback)

    def advance(self, delta_ms: int) -> None:
        if delta_ms < 0:
            raise ValueError("delta_ms must be non-negative")
        self.now_ms += delta_ms
        for callback in self._callbacks:
            callback(self.now_ms)
