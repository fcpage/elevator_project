from __future__ import annotations

import json
import subprocess
from dataclasses import dataclass
from typing import Iterable

from sim.can import CanFrame


@dataclass(frozen=True)
class SupervisorEvent:
    event_type: str
    can_id: int | None = None
    data: list[int] | None = None
    message: str | None = None


def encode_can_rx(can_id: int, data: Iterable[int], timestamp_ms: int) -> str:
    return json.dumps(
        {
            "type": "can_rx",
            "id": can_id,
            "data": list(data),
            "timestamp_ms": timestamp_ms,
        },
        separators=(",", ":"),
    ) + "\n"


def encode_web_request(floor: int) -> str:
    return json.dumps({"type": "web_request", "floor": floor}, separators=(",", ":")) + "\n"


def encode_tick(ms: int) -> str:
    return json.dumps({"type": "tick", "ms": ms}, separators=(",", ":")) + "\n"


def parse_supervisor_line(line: str) -> SupervisorEvent:
    payload = json.loads(line)
    event_type = payload["type"]
    return SupervisorEvent(
        event_type=event_type,
        can_id=payload.get("id"),
        data=payload.get("data"),
        message=payload.get("message"),
    )


class JsonLineSupervisorProcess:
    def __init__(self, command: list[str]) -> None:
        self.command = command
        self.process: subprocess.Popen[str] | None = None

    def start(self) -> None:
        self.process = subprocess.Popen(
            self.command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def stop(self) -> None:
        if self.process is None:
            return
        self.process.terminate()
        self.process.wait(timeout=2)
        self.process = None

    def send_can_rx(self, frame: CanFrame) -> SupervisorEvent | None:
        return self._send_and_read(encode_can_rx(frame.can_id, frame.data, frame.timestamp_ms))

    def send_web_request(self, floor: int) -> SupervisorEvent | None:
        return self._send_and_read(encode_web_request(floor))

    def send_tick(self, ms: int) -> SupervisorEvent | None:
        return self._send_and_read(encode_tick(ms))

    def _send_and_read(self, line: str) -> SupervisorEvent | None:
        if self.process is None or self.process.stdin is None or self.process.stdout is None:
            raise RuntimeError("supervisor process is not running")
        self.process.stdin.write(line)
        self.process.stdin.flush()
        response = self.process.stdout.readline()
        if response == "":
            return None
        return parse_supervisor_line(response)
