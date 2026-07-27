from __future__ import annotations

import json
import sys
import threading
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


QUIET_KINDS = {
    "can.heartbeat",
    "database.poll",
    "diagnostic.snapshot",
    "diagnostic.unchanged",
    "process.output",
    "sql.general_log",
}


@dataclass(frozen=True)
class TraceEvent:
    version: int
    run_id: str
    sequence: int
    wall_time: str
    monotonic_ms: int
    source: str
    kind: str
    severity: str
    payload: dict[str, Any] = field(default_factory=dict)
    correlation_id: str | None = None
    inferred: bool = False


class TraceRecorder:
    def __init__(self, run_id: str, path: Path, *, verbose: bool = False) -> None:
        self.run_id = run_id
        self.path = path
        self.verbose = verbose
        self._started = time.monotonic()
        self._sequence = 0
        self._lock = threading.Lock()
        path.parent.mkdir(parents=True, exist_ok=True)
        self._stream = path.open("a", encoding="utf-8", buffering=1)

    def close(self) -> None:
        with self._lock:
            if not self._stream.closed:
                self._stream.close()

    def emit(
        self,
        source: str,
        kind: str,
        payload: dict[str, Any] | None = None,
        *,
        severity: str = "info",
        correlation_id: str | None = None,
        inferred: bool = False,
        live: bool | None = None,
    ) -> TraceEvent:
        with self._lock:
            self._sequence += 1
            event = TraceEvent(
                version=1,
                run_id=self.run_id,
                sequence=self._sequence,
                wall_time=datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                monotonic_ms=int((time.monotonic() - self._started) * 1000),
                source=source,
                kind=kind,
                severity=severity,
                payload=payload or {},
                correlation_id=correlation_id,
                inferred=inferred,
            )
            self._stream.write(json.dumps(asdict(event), separators=(",", ":"), default=str) + "\n")

        should_print = live if live is not None else self.verbose or kind not in QUIET_KINDS
        if should_print:
            marker = {"error": "ERROR", "warning": "WARN"}.get(severity, "INFO")
            label, message, continuation = _live_message(event)
            correlation = f" [{correlation_id}]" if correlation_id else ""
            destination = sys.stderr if severity == "error" else sys.stdout
            prefix = (
                f"+{event.monotonic_ms / 1000:09.3f} {marker:<5} "
                f"{label:<8} "
            )
            print(f"{prefix}{message}{correlation}", file=destination, flush=True)
            if continuation:
                print(f"{'':>{len(prefix)}}{continuation}", file=destination, flush=True)
        return event


def _live_message(event: TraceEvent) -> tuple[str, str, str | None]:
    payload = event.payload
    kind = event.kind

    if kind == "session.created":
        return (
            "SIM",
            f"run {payload.get('run_id', event.run_id)} created",
            f"trace: {payload.get('trace', '')}",
        )
    if kind == "runtime.ready":
        scenario = payload.get("scenario", "unknown")
        return "SIM", f"ready - {scenario}; waiting for SA", None
    if kind == "transport.connected":
        peer = _format_peer(payload.get("peer"))
        return "LINK", "SA connected" + (f" from {peer}" if peer else ""), None
    if kind == "transport.disconnected":
        return "LINK", "SA disconnected", None
    if kind == "sa.readiness_observed":
        component = _friendly_name(payload.get("component", "component"))
        return "READY", f"{component} observed", None
    if kind == "sa.readiness_waiting":
        pending = ", ".join(_friendly_name(value) for value in payload.get("pending", []))
        return "WAIT", f"waiting for SA: {pending or 'activity'}", None
    if kind == "sa.build.started":
        return "SA", "build started", None
    if kind == "sa.build.ready":
        return "SA", "adapter built", None
    if kind == "sa.runtime.started":
        return "SA", "runtime started", None
    if kind == "sa.database.connecting":
        endpoint = payload.get("endpoint", "")
        return "SA/DB", "connecting" + (f" to {endpoint}" if endpoint else ""), None
    if kind == "sa.database.connected":
        return "SA/DB", "connected", None
    if kind == "sa.database.failed":
        return "SA/DB", "startup failed", str(payload.get("message", "unknown error"))
    if kind == "sa.runtime.failed":
        return "SA", "runtime failed", str(payload.get("message", "unknown error"))
    if kind == "testpoint.hit":
        name = payload.get("name", "unnamed")
        detail = str(payload.get("detail", "")).strip()
        thread_id = payload.get("thread_id")
        continuation = detail
        if thread_id is not None:
            continuation = f"{detail} (thread {thread_id})" if detail else f"thread {thread_id}"
        return "TEST", str(name), continuation or None
    if kind == "state.changed":
        state = payload.get("state", "Unknown")
        floor = payload.get("floor", "?")
        target = payload.get("target", "?")
        direction = payload.get("direction", "None")
        door = "open" if payload.get("door_open") else "closed"
        return "STATE", str(state), f"floor {floor} -> {target}; {direction}; door {door}"

    details = _format_payload(payload)
    return event.source.upper()[:8], kind, details.lstrip() or None


def _friendly_name(value: Any) -> str:
    return str(value).replace("_", " ")


def _format_peer(value: Any) -> str:
    if isinstance(value, (list, tuple)) and len(value) >= 2:
        return f"{value[0]}:{value[1]}"
    return str(value) if value else ""


def _format_payload(payload: dict[str, Any]) -> str:
    if not payload:
        return ""
    compact = " ".join(f"{key}={value}" for key, value in payload.items())
    return f" {compact}"
