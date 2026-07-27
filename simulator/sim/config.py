from __future__ import annotations

import json
import os
import platform
from dataclasses import asdict, dataclass
from pathlib import Path


APP_DIRECTORY_NAME = "Project6ElevatorSimulator"
DEFAULT_DB_PORT = 3307
DEFAULT_CAN_PORT = 8765
DEFAULT_DIAGNOSTICS_PORT = 8766


def state_directory() -> Path:
    if os.name == "nt":
        base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
        return base / APP_DIRECTORY_NAME
    xdg_state = os.environ.get("XDG_STATE_HOME")
    base = Path(xdg_state) if xdg_state else Path.home() / ".local" / "state"
    return base / "project6-elevator-simulator"


@dataclass(frozen=True)
class SimulatorPaths:
    root: Path
    database: Path
    runs: Path
    runtime: Path
    venv: Path

    @classmethod
    def discover(cls) -> "SimulatorPaths":
        root = state_directory()
        return cls(
            root=root,
            database=root / "mariadb",
            runs=root / "runs",
            runtime=root / "runtime",
            venv=root / "venv",
        )

    def create(self) -> None:
        for path in (self.root, self.database, self.runs, self.runtime):
            path.mkdir(parents=True, exist_ok=True)


@dataclass(frozen=True)
class ActiveSession:
    version: int
    run_id: str
    transport: str
    can_host: str
    can_port: int
    diagnostics_host: str
    diagnostics_port: int
    can_interface: str
    database_url: str
    database_user: str
    database_password: str
    database_schema: str
    sa_log_path: str
    trace_path: str
    simulator_pid: int
    created_at: str
    mode: str = "full"
    platform: str = platform.system()

    def write(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(".tmp")
        temporary.write_text(json.dumps(asdict(self), indent=2), encoding="utf-8")
        temporary.replace(path)


def session_path(paths: SimulatorPaths | None = None) -> Path:
    resolved = paths or SimulatorPaths.discover()
    return resolved.runtime / "active-session.json"
