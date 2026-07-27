from __future__ import annotations

import argparse
import asyncio
import contextlib
import json
import os
import platform
import shutil
import signal
import sys
import uuid
from datetime import datetime
from pathlib import Path

from sim.config import (
    DEFAULT_CAN_PORT,
    DEFAULT_DB_PORT,
    DEFAULT_DIAGNOSTICS_PORT,
    ActiveSession,
    SimulatorPaths,
    session_path,
)
from sim.engine import PersistentSimulator
from sim.mariadb_runtime import DatabaseObserver, IsolatedMariaDb, find_executable
from sim.scenario import load_scenario
from sim.schema_profile import load_schema_profile
from sim.trace import TraceRecorder
from sim.transport import DiagnosticsServer, LoopbackCanTransport, SocketCanTransport


SIMULATOR_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SCENARIO = SIMULATOR_ROOT / "scenarios" / "healthy_loop.json"
SCHEMA_DIRECTORY = SIMULATOR_ROOT / "schema"
DEFAULT_SCHEMA_PROFILE = "agreed-v1"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Project 6 persistent elevator simulator")
    subcommands = parser.add_subparsers(dest="command", required=True)

    setup = subcommands.add_parser("setup", help="Initialize the simulator-owned MariaDB")
    setup.add_argument("--check-only", action="store_true")
    setup.add_argument("--schema-profile", default=DEFAULT_SCHEMA_PROFILE)
    setup.add_argument(
        "--recreate-schema",
        action="store_true",
        help="Drop and recreate only the selected schema in the simulator-owned MariaDB.",
    )

    doctor = subcommands.add_parser("doctor", help="Check system prerequisites")
    doctor.add_argument("--json", action="store_true")

    run = subcommands.add_parser("run", help="Run a persistent simulator scenario")
    run.add_argument("--scenario", type=Path, default=DEFAULT_SCENARIO)
    run.add_argument("--schema-profile", default=DEFAULT_SCHEMA_PROFILE)
    run.add_argument("--transport", choices=("auto", "loopback", "socketcan"), default="auto")
    run.add_argument("--can-interface", default="vcan0")
    run.add_argument("--verbose", action="store_true")
    run.add_argument(
        "--plant-only",
        action="store_true",
        help="Run only the CAN elevator plant; do not start MariaDB, GUI traffic, or diagnostics.",
    )

    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.command == "doctor":
        return _doctor(as_json=args.json)
    if args.command == "setup":
        return _setup(
            check_only=args.check_only,
            schema_profile=args.schema_profile,
            recreate_schema=args.recreate_schema,
        )
    if args.command == "run":
        try:
            return asyncio.run(_run(args))
        except KeyboardInterrupt:
            return 0
    return 2


def _doctor(*, as_json: bool = False) -> int:
    connector_header, connector_library = _find_mysql_connector()
    tools = {
        "python": sys.executable,
        "cmake": shutil.which("cmake"),
        "compiler": shutil.which("cl") or shutil.which("clang++") or shutil.which("g++"),
        "mariadbd": find_executable("mariadbd", "mysqld"),
        "mariadb_installer": find_executable("mariadb-install-db", "mysql_install_db"),
        "mysqlcppconn_header": connector_header,
        "mysqlcppconn_library": connector_library,
    }
    if platform.system() == "Linux":
        tools["ip"] = shutil.which("ip")
    missing = [name for name, value in tools.items() if value is None]
    payload = {"tools": tools, "missing": missing}
    if as_json:
        print(json.dumps(payload, indent=2))
    else:
        for name, value in tools.items():
            print(f"{name:20} {value or 'MISSING'}")
        if missing:
            print()
            print(_install_guidance(missing))
    return 1 if missing else 0


def _setup(*, check_only: bool, schema_profile: str, recreate_schema: bool) -> int:
    if _doctor(as_json=False) != 0:
        return 1
    paths = SimulatorPaths.discover()
    paths.create()
    profile = load_schema_profile(SCHEMA_DIRECTORY, schema_profile)
    database = IsolatedMariaDb(paths, profile, port=DEFAULT_DB_PORT)
    if check_only:
        print(f"Simulator state directory: {paths.root}")
        return 0
    try:
        database.initialize()
        database.start(recreate_schema=recreate_schema)
        observer = DatabaseObserver(database, profile)
        observer.reset()
    finally:
        database.stop()
    action = "recreated" if recreate_schema else "initialized"
    print(f"Simulator MariaDB {action} with schema profile {profile.name!r} under {paths.database}")
    return 0


async def _run(args: argparse.Namespace) -> int:
    paths = SimulatorPaths.discover()
    paths.create()
    scenario_path = args.scenario
    if not scenario_path.is_absolute():
        scenario_path = (Path.cwd() / scenario_path).resolve()
    scenario = load_scenario(scenario_path)
    profile = load_schema_profile(SCHEMA_DIRECTORY, args.schema_profile)
    database: IsolatedMariaDb | None = None
    observer: DatabaseObserver | None = None
    if not args.plant_only:
        database = IsolatedMariaDb(paths, profile, port=DEFAULT_DB_PORT)
        missing = database.preflight()
        if missing:
            raise RuntimeError(f"missing MariaDB tools: {', '.join(missing)}; run simulator setup")
        database.initialize()
        database.start()
        observer = DatabaseObserver(database, profile)
        observer.reset()

    run_id = datetime.now().strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:6]
    run_dir = paths.runs / run_id
    trace_path = run_dir / "trace.ndjson"
    sa_log_path = run_dir / "supervisory.log"
    trace = TraceRecorder(run_id, trace_path, verbose=args.verbose)

    transport_name = args.transport
    if transport_name == "auto":
        transport_name = "loopback" if os.name == "nt" else "socketcan"
    if transport_name == "loopback":
        transport = LoopbackCanTransport("127.0.0.1", DEFAULT_CAN_PORT)
    else:
        transport = SocketCanTransport(args.can_interface)
    diagnostics = None if args.plant_only else DiagnosticsServer("127.0.0.1", DEFAULT_DIAGNOSTICS_PORT)

    session = ActiveSession(
        version=1,
        run_id=run_id,
        transport=transport_name,
        can_host="127.0.0.1",
        can_port=DEFAULT_CAN_PORT,
        diagnostics_host="127.0.0.1",
        diagnostics_port=DEFAULT_DIAGNOSTICS_PORT,
        can_interface=args.can_interface,
        database_url="" if args.plant_only else f"tcp://127.0.0.1:{DEFAULT_DB_PORT}",
        database_user="" if args.plant_only else "pi",
        database_password="" if args.plant_only else "ese",
        database_schema="" if args.plant_only else profile.schema_name,
        sa_log_path=str(sa_log_path),
        trace_path=str(trace_path),
        simulator_pid=os.getpid(),
        created_at=datetime.now().astimezone().isoformat(timespec="seconds"),
        mode="plant_only" if args.plant_only else "full",
    )
    session.write(session_path(paths))
    trace.emit(
        "simulator",
        "session.created",
        {
            "run_id": run_id,
            "transport": transport_name,
            "trace": str(trace_path),
            "mode": "plant_only" if args.plant_only else "full",
            "next": "connect your CAN application" if args.plant_only else "start the SA with simulator/run-sa",
        },
    )

    simulator = PersistentSimulator(
        transport=transport,
        diagnostics=diagnostics,
        database=database,
        database_observer=observer,
        scenario=scenario,
        trace=trace,
        sa_log_path=sa_log_path,
        plant_only=args.plant_only,
    )
    loop = asyncio.get_running_loop()
    for signal_name in (signal.SIGINT, signal.SIGTERM):
        with contextlib.suppress(NotImplementedError):
            loop.add_signal_handler(signal_name, simulator.stop_requested.set)
    try:
        await simulator.run()
    finally:
        if database is not None:
            database.stop()
        trace.close()
    return 0


def _install_guidance(missing: list[str]) -> str:
    if platform.system() == "Windows":
        return (
            "Install missing system prerequisites, then rerun setup:\n"
            "  winget install Python.Python.3.12\n"
            "  winget install Kitware.CMake\n"
            "  winget install MariaDB.Server\n"
            "A C++20 compiler and MySQL Connector/C++ development package are also required."
        )
    return (
        "Install missing system prerequisites, then rerun setup:\n"
        "  sudo apt install python3 python3-venv cmake g++ mariadb-server "
        "libmysqlcppconn-dev iproute2 can-utils"
    )


def _find_mysql_connector() -> tuple[str | None, str | None]:
    configured_include = os.environ.get("MYSQLCPPCONN_INCLUDE_DIR")
    configured_library = os.environ.get("MYSQLCPPCONN_LIBRARY")
    header: Path | None = None
    library: Path | None = None
    if configured_include:
        candidate = Path(configured_include) / "mysql_driver.h"
        if candidate.exists():
            header = candidate
    if configured_library and Path(configured_library).exists():
        library = Path(configured_library)

    if os.name == "nt":
        program_files = Path(os.environ.get("ProgramFiles", "C:/Program Files"))
        install_roots = (
            *program_files.glob("MySQL/MySQL Connector C++ *"),
            *program_files.glob("MySQL/Connector C++ *"),
        )
        if header is None:
            for root in install_roots:
                header = next(iter(root.glob("include/**/mysql_driver.h")), None)
                if header is not None:
                    break
        if library is None:
            for root in install_roots:
                candidates = (
                    *root.glob("lib64/**/mysqlcppconn.lib"),
                    *root.glob("lib/**/mysqlcppconn.lib"),
                )
                if candidates:
                    library = candidates[0]
                    break
    else:
        if header is None:
            for candidate in (
                Path("/usr/include/mysql_driver.h"),
                Path("/usr/include/mysql-cppconn/jdbc/mysql_driver.h"),
            ):
                if candidate.exists():
                    header = candidate
                    break
        if library is None:
            for root in (Path("/usr/lib"), Path("/usr/lib/x86_64-linux-gnu")):
                matches = tuple(root.glob("libmysqlcppconn.so*"))
                if matches:
                    library = matches[0]
                    break
    return (
        str(header.parent) if header is not None else None,
        str(library) if library is not None else None,
    )


if __name__ == "__main__":
    raise SystemExit(main())
