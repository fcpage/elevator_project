from __future__ import annotations

import asyncio
import contextlib
import json
import re
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from sim.can import CanFrame
from sim.mariadb_runtime import DatabaseObserver, IsolatedMariaDb
from sim.protocol import (
    CC_CAN_ID,
    EC_CAN_ID,
    FLOOR_CAN_IDS,
    HB_OK,
    HB_SC_REQUEST,
    SC_CAN_ID,
    SC_DOOR_CLOSE,
    SC_DOOR_OPEN,
    STATUS_OR_ENABLE_MASK,
    describe_frame,
    floor_from_payload,
    is_internal_payload,
)
from sim.scenario import Scenario, ScenarioStep
from sim.trace import TraceRecorder
from sim.transport import DiagnosticsServer, TransportEvent


@dataclass
class FaultState:
    suppressed_heartbeat_node: int | None = None
    suppressed_heartbeat_cycles: int = 0
    next_arrival_delay_ms: int = 0
    drop_direction: str | None = None
    drop_can_id: int | None = None
    drop_data_byte: int | None = None


class PersistentSimulator:
    def __init__(
        self,
        *,
        transport: Any,
        diagnostics: DiagnosticsServer | None,
        database: IsolatedMariaDb | None,
        database_observer: DatabaseObserver | None,
        scenario: Scenario,
        trace: TraceRecorder,
        sa_log_path: Path,
        heartbeat_delay_ms: int = 50,
        travel_ms_per_floor: int = 2000,
        plant_only: bool = False,
    ) -> None:
        self.transport = transport
        self.diagnostics = diagnostics
        self.database = database
        self.database_observer = database_observer
        self.scenario = scenario
        self.trace = trace
        self.sa_log_path = sa_log_path
        self.heartbeat_delay_ms = heartbeat_delay_ms
        self.travel_ms_per_floor = travel_ms_per_floor
        self.plant_only = plant_only
        self.current_floor = 1
        self.target_floor = 1
        self.doors_open = False
        self.faults = FaultState()
        self.stop_requested = asyncio.Event()
        self._history: list[dict[str, Any]] = []
        self._history_changed = asyncio.Condition()
        self._tasks: list[asyncio.Task[Any]] = []
        self._movement_task: asyncio.Task[None] | None = None
        self._active_correlation: str | None = None
        self.degraded = False
        self._last_diagnostic_time = time.monotonic()
        self._diagnostic_gap_reported = False
        self._last_sa_sql_time = time.monotonic()
        self._sql_gap_reported = False
        self._unique_gui_rows = 0
        self._last_reprocessing_reads = 0
        self._last_can_fault: Any = None
        self._last_database_fault: Any = None
        self._last_heartbeat_missed = 0
        self._sa_transport_ready = asyncio.Event()
        self._sa_database_ready = asyncio.Event()

    async def run(self) -> None:
        await self.transport.start()
        if self.diagnostics is not None:
            await self.diagnostics.start()
        self.trace.emit(
            "simulator",
            "runtime.ready",
            {
                "scenario": self.scenario.name,
                "waiting_for_sa": True,
                "mode": "plant_only" if self.plant_only else "full",
            },
        )
        self._tasks = [
            asyncio.create_task(self._transport_loop(), name="transport"),
            asyncio.create_task(self._sa_log_loop(), name="sa-log"),
        ]
        if not self.plant_only:
            self._tasks.extend(
                [
                    asyncio.create_task(self._diagnostics_loop(), name="diagnostics"),
                    asyncio.create_task(self._database_loop(), name="database-observer"),
                    asyncio.create_task(self._general_log_loop(), name="general-log"),
                    asyncio.create_task(self._health_monitor_loop(), name="health-monitor"),
                    asyncio.create_task(self._scenario_loop(), name="scenario"),
                ]
            )
        else:
            self._tasks.append(asyncio.create_task(self._scenario_loop(), name="scenario"))
            self.trace.emit(
                "simulator",
                "plant.ready",
                {"heartbeat_ms": self.heartbeat_delay_ms, "travel_ms_per_floor": self.travel_ms_per_floor},
            )
        await self.stop_requested.wait()
        await self.shutdown()

    async def shutdown(self) -> None:
        if self._movement_task is not None:
            self._movement_task.cancel()
        for task in self._tasks:
            task.cancel()
        await asyncio.gather(*self._tasks, return_exceptions=True)
        await self.transport.stop()
        if self.diagnostics is not None:
            await self.diagnostics.stop()

    async def _scenario_loop(self) -> None:
        await self.transport.ready.wait()
        # CAN readiness is sufficient for physical plant journeys. Database
        # readiness is required only by journeys that inject GUI rows; a branch
        # without a database worker must still be able to exercise FC/CC/EC.
        await self._wait_for_sa_readiness(require_database=False)
        journeys = self.scenario.journeys
        if self.plant_only:
            journeys = tuple(
                journey
                for journey in journeys
                if all(step.action not in {"gui_request", "database_outage"} for step in journey)
            )
            if not journeys:
                self.trace.emit(
                    "scenario",
                    "scenario.plant_only_no_can_journey",
                    {"name": self.scenario.name},
                    severity="warning",
                )
                return
            if len(journeys) != len(self.scenario.journeys):
                self.trace.emit(
                    "scenario",
                    "scenario.plant_only_filtered",
                    {"name": self.scenario.name, "journeys": len(journeys)},
                )
        self.trace.emit("scenario", "scenario.started", {"name": self.scenario.name})
        pass_number = 0
        journey_number = 0
        while not self.stop_requested.is_set() and not self.degraded:
            pass_number += 1
            for journey_index, steps in enumerate(journeys, start=1):
                if (
                    not self.plant_only
                    and any(step.action == "gui_request" for step in steps)
                    and not self._sa_database_ready.is_set()
                ):
                    self.trace.emit(
                        "scenario",
                        "scenario.journey_skipped_database_unready",
                        {"pass": pass_number, "journey": journey_index},
                        severity="warning",
                    )
                    continue
                journey_number += 1
                correlation = f"journey-{journey_number:04d}-{uuid.uuid4().hex[:6]}"
                self._active_correlation = correlation
                self.trace.emit(
                    "scenario",
                    "journey.started",
                    {"pass": pass_number, "journey": journey_index},
                    correlation_id=correlation,
                )
                cursor = len(self._history)
                for step in steps:
                    try:
                        cursor = await self._execute_step(step, cursor, correlation)
                    except TimeoutError as error:
                        self.degraded = True
                        self.trace.emit(
                            "anomaly",
                            "scenario.expectation_timeout",
                            {
                                "step": step.action,
                                "message": str(error),
                                "injection_paused": True,
                            },
                            severity="error",
                            correlation_id=correlation,
                        )
                        break
                    except Exception as error:
                        self.degraded = True
                        self.trace.emit(
                            "anomaly",
                            "scenario.failed",
                            {
                                "step": step.action,
                                "message": str(error),
                                "injection_paused": True,
                            },
                            severity="error",
                            correlation_id=correlation,
                        )
                        break
                if self.degraded:
                    break
                self.trace.emit(
                    "scenario",
                    "journey.completed",
                    {"pass": pass_number, "journey": journey_index},
                    correlation_id=correlation,
                )
            if self.degraded:
                break
            if not self.scenario.repeat:
                break
        self._active_correlation = None

    async def _wait_for_sa_readiness(self, *, require_database: bool = True) -> None:
        pending = {"can_activity": self._sa_transport_ready}
        if require_database and not self.plant_only:
            pending["database_connection"] = self._sa_database_ready
        next_report = time.monotonic() + 5.0
        while pending and not self.stop_requested.is_set():
            for name, event in tuple(pending.items()):
                if event.is_set():
                    self.trace.emit("simulator", "sa.readiness_observed", {"component": name})
                    del pending[name]
            if not pending:
                return
            if time.monotonic() >= next_report:
                self.trace.emit(
                    "simulator",
                    "sa.readiness_waiting",
                    {"pending": sorted(pending)},
                    severity="warning",
                )
                next_report = time.monotonic() + 5.0
            try:
                await asyncio.wait_for(self.stop_requested.wait(), timeout=0.1)
            except asyncio.TimeoutError:
                pass

    async def _execute_step(
        self,
        step: ScenarioStep,
        cursor: int,
        correlation: str,
    ) -> int:
        args = step.arguments
        if step.action == "gui_request":
            floor = int(args["floor"])
            await asyncio.to_thread(
                self.database_observer.insert_gui_request,
                floor,
                int(args.get("remote", 0)),
            )
            self.trace.emit(
                "gui",
                "database.request_inserted",
                {"floor": floor, "remote": int(args.get("remote", 0))},
                correlation_id=correlation,
            )
            return cursor

        if step.action == "floor_call":
            floor = int(args["floor"])
            await self._send_can(
                CanFrame(can_id=FLOOR_CAN_IDS[floor], data=[1], source=f"floor_{floor}"),
                correlation,
            )
            return cursor

        if step.action == "car_request":
            floor = int(args["floor"])
            await self._send_can(
                CanFrame(can_id=CC_CAN_ID, data=[floor], source="car_controller"),
                correlation,
            )
            return cursor

        if step.action == "wait":
            await asyncio.sleep(int(args["ms"]) / 1000)
            return cursor

        if step.action == "wait_for_can":
            expected_id = _parse_int(args.get("id"))
            expected_data = _parse_int(args.get("data_byte"))
            timeout = int(args.get("timeout_ms", 10_000)) / 1000

            def matches(event: dict[str, Any]) -> bool:
                if event.get("kind") != "can.tx":
                    return False
                if expected_id is not None and event.get("id") != expected_id:
                    return False
                data = event.get("data", [])
                return expected_data is None or bool(data) and data[0] == expected_data

            return await self._wait_for(matches, cursor, timeout, "expected CAN frame")

        if step.action == "wait_for_database":
            table = str(args["table"])
            expected = dict(args.get("fields", {}))
            timeout = int(args.get("timeout_ms", 10_000)) / 1000

            def matches(event: dict[str, Any]) -> bool:
                if event.get("kind") != "database.row" or event.get("table") != table:
                    return False
                values = event.get("values", {})
                return all(values.get(key) == value for key, value in expected.items())

            return await self._wait_for(matches, cursor, timeout, f"database row in {table}")

        if step.action == "suppress_heartbeat":
            self.faults.suppressed_heartbeat_node = _parse_int(args["node_id"])
            self.faults.suppressed_heartbeat_cycles = int(args.get("cycles", 1))
            self.trace.emit(
                "fault",
                "fault.armed",
                {
                    "type": "suppress_heartbeat",
                    "node_id": hex(self.faults.suppressed_heartbeat_node),
                    "cycles": self.faults.suppressed_heartbeat_cycles,
                },
                severity="warning",
                correlation_id=correlation,
            )
            return cursor

        if step.action == "delay_arrival":
            self.faults.next_arrival_delay_ms = int(args["delay_ms"])
            self.trace.emit(
                "fault",
                "fault.armed",
                {"type": "delay_arrival", "delay_ms": self.faults.next_arrival_delay_ms},
                severity="warning",
                correlation_id=correlation,
            )
            return cursor

        if step.action == "drop_next_frame":
            self.faults.drop_direction = str(args["direction"])
            self.faults.drop_can_id = _parse_int(args["id"])
            self.faults.drop_data_byte = _parse_int(args.get("data_byte"))
            self.trace.emit(
                "fault",
                "fault.armed",
                {
                    "type": "drop_next_frame",
                    "direction": self.faults.drop_direction,
                    "id": hex(self.faults.drop_can_id),
                    "data_byte": self.faults.drop_data_byte,
                },
                severity="warning",
                correlation_id=correlation,
            )
            return cursor

        if step.action == "database_outage":
            duration = int(args["duration_ms"]) / 1000
            self.trace.emit(
                "fault",
                "database.outage_started",
                {"duration_ms": int(duration * 1000)},
                severity="warning",
                correlation_id=correlation,
            )
            await asyncio.to_thread(self.database.reconnect_after, duration)
            self.trace.emit(
                "fault",
                "database.outage_ended",
                {},
                severity="warning",
                correlation_id=correlation,
            )
            return cursor

        raise ValueError(f"unhandled scenario action: {step.action}")

    async def _transport_loop(self) -> None:
        while True:
            event: TransportEvent = await self.transport.events.get()
            if event.kind == "can.tx":
                self._sa_transport_ready.set()
                message = event.payload
                frame = CanFrame(
                    can_id=int(message["id"]),
                    data=[int(byte) for byte in message.get("data", [])],
                    source="supervisor",
                    timestamp_ms=int(message.get("timestamp_ms", _monotonic_ms())),
                )
                if self._should_drop("sa_to_sim", frame):
                    self.trace.emit(
                        "fault",
                        "can.frame_dropped",
                        {"direction": "sa_to_sim", "id": hex(frame.can_id), "data": frame.data},
                        severity="warning",
                        correlation_id=self._active_correlation,
                    )
                    continue
                await self._record_can(frame, "sa_to_sim")
                await self._handle_supervisor_frame(frame)
            elif event.kind.endswith("disconnected"):
                self.trace.emit("transport", event.kind, event.payload, severity="warning")
            elif event.kind.endswith("error"):
                self.trace.emit("transport", event.kind, event.payload, severity="error")
            else:
                self.trace.emit("transport", event.kind, event.payload)

    async def _diagnostics_loop(self) -> None:
        last_payload: dict[str, Any] | None = None
        last_control: dict[str, Any] | None = None
        while True:
            event = await self.diagnostics.events.get()
            payload = event.payload
            self._last_diagnostic_time = time.monotonic()
            if self._diagnostic_gap_reported:
                self.trace.emit("anomaly", "diagnostic.stream_recovered", {})
                self._diagnostic_gap_reported = False
            if payload.get("type") == "testpoint":
                testpoint = {
                    key: value
                    for key, value in payload.items()
                    if key not in {"type", "version"}
                }
                self.trace.emit("sa", "testpoint.hit", testpoint)
                await self._append_history({"kind": "testpoint.hit", **testpoint})
                continue
            changed = payload != last_payload
            last_payload = payload
            kind = "diagnostic.snapshot" if changed else "diagnostic.unchanged"
            severity = "error" if payload.get("control", {}).get("faulted") else "info"
            self.trace.emit("sa", kind, payload, severity=severity)
            control = payload.get("control", {})
            if control != last_control:
                self.trace.emit(
                    "sa",
                    "state.changed",
                    {
                        "state": control.get("state"),
                        "floor": control.get("current_floor"),
                        "target": control.get("target_floor"),
                        "direction": control.get("direction"),
                        "door_open": control.get("door_open"),
                        "faulted": control.get("faulted"),
                    },
                    severity=severity,
                )
                last_control = dict(control)
            await self._append_history({"kind": "diagnostic", **payload})
            loop = payload.get("loop", {})
            if int(loop.get("overrun_ms", 0)) > 0:
                self.trace.emit(
                    "anomaly",
                    "control.loop_overrun",
                    {"overrun_ms": loop["overrun_ms"], "sequence": loop.get("sequence")},
                    severity="warning",
                )
            can = payload.get("can", {})
            database = payload.get("database", {})
            can_fault = can.get("fault")
            database_fault = database.get("fault")
            if can_fault not in (None, 0) and can_fault != self._last_can_fault:
                self.trace.emit(
                    "anomaly",
                    "can.fault_changed",
                    {"fault": can_fault, "dropped": can.get("dropped"), "tx_failed": can.get("tx_failed")},
                    severity="error",
                )
            if database_fault not in (None, 0) and database_fault != self._last_database_fault:
                self.trace.emit(
                    "anomaly",
                    "database.fault_changed",
                    {"fault": database_fault, "dropped": database.get("dropped"), "write_failed": database.get("write_failed")},
                    severity="error",
                )
            self._last_can_fault = can_fault
            self._last_database_fault = database_fault
            heartbeat_missed = int(can.get("hb_missed", 0))
            if heartbeat_missed > 0 and heartbeat_missed != self._last_heartbeat_missed:
                self.trace.emit(
                    "anomaly",
                    "heartbeat.missed",
                    {"mask": heartbeat_missed},
                    severity="error",
                )
            self._last_heartbeat_missed = heartbeat_missed
            read_count = int(database.get("reads", 0))
            if (
                self._unique_gui_rows > 0
                and read_count > self._unique_gui_rows
                and self._last_reprocessing_reads == 0
            ):
                self._last_reprocessing_reads = read_count
                self.trace.emit(
                    "anomaly",
                    "database.possible_reprocessing",
                    {"database_reads": read_count, "unique_gui_rows": self._unique_gui_rows},
                    severity="warning",
                )

    async def _database_loop(self) -> None:
        last_error: str | None = None
        while True:
            try:
                rows = await asyncio.to_thread(self.database_observer.poll)
                if last_error is not None:
                    self.trace.emit("database", "database.poll_recovered", {})
                    last_error = None
                for row in rows:
                    payload = {
                        "table": row.table,
                        "index": row.index,
                        "values": _json_values(row.values),
                    }
                    self.trace.emit(
                        "database",
                        "database.row",
                        payload,
                        correlation_id=self._active_correlation,
                        inferred=self._active_correlation is not None,
                    )
                    if row.table == self.database_observer.profile.inbound_table:
                        self._unique_gui_rows += 1
                    await self._append_history({"kind": "database.row", **payload})
            except Exception as error:
                message = str(error)
                if message != last_error:
                    self.trace.emit(
                        "database",
                        "database.poll_error",
                        {"message": message},
                        severity="warning",
                    )
                    last_error = message
            await asyncio.sleep(0.2)

    async def _general_log_loop(self) -> None:
        path = self.database.general_log_path
        position = path.stat().st_size if path.exists() else 0
        observer_threads: set[str] = set()
        sa_threads: set[str] = set()
        while True:
            if not path.exists():
                await asyncio.sleep(0.5)
                continue
            if path.stat().st_size < position:
                position = 0
                observer_threads.clear()
                sa_threads.clear()
            with path.open("r", encoding="utf-8", errors="replace") as stream:
                stream.seek(position)
                lines = stream.readlines()
                position = stream.tell()
            for line in lines:
                if not line.strip():
                    continue
                thread_match = re.search(r"\s(\d+)\s+(Connect|Query|Prepare|Execute|Quit)\s", line)
                thread_id = thread_match.group(1) if thread_match else None
                if "elevator_sim_observer" in line and thread_id is not None:
                    observer_threads.add(thread_id)
                    continue
                if "Connect" in line and "pi@" in line and thread_id is not None:
                    sa_threads.add(thread_id)
                    self._sa_database_ready.set()
                    self._last_sa_sql_time = time.monotonic()
                    self.trace.emit(
                        "database",
                        "sql.general_log",
                        {"line": line.rstrip()},
                        live=False,
                    )
                    continue
                if thread_id in observer_threads:
                    if thread_match and thread_match.group(2) == "Quit":
                        observer_threads.discard(thread_id)
                    continue
                if thread_id in sa_threads:
                    self._last_sa_sql_time = time.monotonic()
                    self._sql_gap_reported = False
                    self.trace.emit(
                        "database",
                        "sql.general_log",
                        {"line": line.rstrip()},
                        live=False,
                    )
                    if thread_match and thread_match.group(2) == "Quit":
                        sa_threads.discard(thread_id)
            await asyncio.sleep(0.25)

    async def _sa_log_loop(self) -> None:
        position = 0
        while True:
            if not self.sa_log_path.exists():
                await asyncio.sleep(0.25)
                continue
            if self.sa_log_path.stat().st_size < position:
                position = 0
            with self.sa_log_path.open("r", encoding="utf-8", errors="replace") as stream:
                stream.seek(position)
                lines = stream.readlines()
                position = stream.tell()
            for line in lines:
                text = line.strip()
                if text:
                    lowered = text.lower()
                    severity = (
                        "error"
                        if "error" in lowered or "failed" in lowered
                        else "info"
                    )
                    self.trace.emit("sa-log", "process.output", {"line": text}, severity=severity)
                    live_event = _classify_sa_output(text)
                    if live_event is not None:
                        source, kind, payload, live_severity = live_event
                        self.trace.emit(
                            source,
                            kind,
                            payload,
                            severity=live_severity,
                        )
            await asyncio.sleep(0.1)

    async def _health_monitor_loop(self) -> None:
        while True:
            now = time.monotonic()
            if (
                self.diagnostics.ready.is_set()
                and now - self._last_diagnostic_time > 1.0
                and not self._diagnostic_gap_reported
            ):
                self._diagnostic_gap_reported = True
                self.trace.emit(
                    "anomaly",
                    "diagnostic.stream_stale",
                    {"stale_ms": int((now - self._last_diagnostic_time) * 1000)},
                    severity="warning",
                )
            if (
                self.diagnostics.ready.is_set()
                and now - self._last_sa_sql_time > 2.5
                and not self._sql_gap_reported
            ):
                self._sql_gap_reported = True
                self.trace.emit(
                    "anomaly",
                    "database.sql_activity_stale",
                    {"stale_ms": int((now - self._last_sa_sql_time) * 1000)},
                    severity="warning",
                )
            await asyncio.sleep(0.25)

    async def _handle_supervisor_frame(self, frame: CanFrame) -> None:
        if not frame.data:
            return
        payload = frame.data[0]
        if frame.can_id == SC_CAN_ID and payload == HB_SC_REQUEST:
            await self._schedule_heartbeat_replies()
            return
        if frame.can_id == SC_CAN_ID and payload == SC_DOOR_OPEN:
            self.doors_open = True
            return
        if frame.can_id == SC_CAN_ID and payload == SC_DOOR_CLOSE:
            self.doors_open = False
            return
        if frame.can_id == SC_CAN_ID and not is_internal_payload(payload):
            if (payload & STATUS_OR_ENABLE_MASK) == 0:
                return
            target = floor_from_payload(payload)
            if target not in (1, 2, 3):
                return
            self.target_floor = target
            if self._movement_task is not None:
                self._movement_task.cancel()
            self._movement_task = asyncio.create_task(self._move_elevator(target))

    async def _move_elevator(self, target: int) -> None:
        await self._send_can(
            CanFrame(
                can_id=EC_CAN_ID,
                data=[STATUS_OR_ENABLE_MASK | self.current_floor],
                source="elevator_controller",
            ),
            self._active_correlation,
        )
        travel_ms = abs(target - self.current_floor) * self.travel_ms_per_floor
        travel_ms += self.faults.next_arrival_delay_ms
        self.faults.next_arrival_delay_ms = 0
        await asyncio.sleep(travel_ms / 1000)
        self.current_floor = target
        await self._send_can(
            CanFrame(can_id=EC_CAN_ID, data=[target], source="elevator_controller"),
            self._active_correlation,
        )

    async def _schedule_heartbeat_replies(self) -> None:
        await asyncio.sleep(self.heartbeat_delay_ms / 1000)
        suppressed = self.faults.suppressed_heartbeat_node
        for node_id in (CC_CAN_ID, FLOOR_CAN_IDS[1], FLOOR_CAN_IDS[2], FLOOR_CAN_IDS[3]):
            if suppressed == node_id and self.faults.suppressed_heartbeat_cycles > 0:
                self.trace.emit(
                    "fault",
                    "heartbeat.reply_suppressed",
                    {"node_id": hex(node_id)},
                    severity="warning",
                )
                continue
            await self._send_can(
                CanFrame(can_id=node_id, data=[HB_OK], source=f"node_{node_id:03x}"),
                None,
            )
        if suppressed is not None and self.faults.suppressed_heartbeat_cycles > 0:
            self.faults.suppressed_heartbeat_cycles -= 1
            if self.faults.suppressed_heartbeat_cycles == 0:
                self.faults.suppressed_heartbeat_node = None

    async def _send_can(self, frame: CanFrame, correlation: str | None) -> None:
        if self._should_drop("sim_to_sa", frame):
            self.trace.emit(
                "fault",
                "can.frame_dropped",
                {"direction": "sim_to_sa", "id": hex(frame.can_id), "data": frame.data},
                severity="warning",
                correlation_id=correlation,
            )
            return
        await self.transport.send(frame)
        await self._record_can(frame, "sim_to_sa", correlation)

    async def _record_can(
        self,
        frame: CanFrame,
        direction: str,
        correlation: str | None = None,
    ) -> None:
        description = describe_frame(frame)
        kind = "can.heartbeat" if "heartbeat" in description else "can.frame"
        payload = {
            "direction": direction,
            "id": frame.can_id,
            "id_hex": hex(frame.can_id),
            "data": frame.data,
            "decoded": description,
        }
        self.trace.emit(
            "can",
            kind,
            payload,
            correlation_id=correlation or self._active_correlation,
            inferred=correlation is None and self._active_correlation is not None,
        )
        await self._append_history({"kind": "can.tx" if direction == "sa_to_sim" else "can.rx", **payload})

    def _should_drop(self, direction: str, frame: CanFrame) -> bool:
        if (
            self.faults.drop_direction != direction
            or self.faults.drop_can_id != frame.can_id
            or (
                self.faults.drop_data_byte is not None
                and (not frame.data or frame.data[0] != self.faults.drop_data_byte)
            )
        ):
            return False
        self.faults.drop_direction = None
        self.faults.drop_can_id = None
        self.faults.drop_data_byte = None
        return True

    async def _append_history(self, event: dict[str, Any]) -> None:
        async with self._history_changed:
            self._history.append(event)
            self._history_changed.notify_all()

    async def _wait_for(
        self,
        predicate: Any,
        cursor: int,
        timeout: float,
        description: str,
    ) -> int:
        deadline = time.monotonic() + timeout
        while True:
            for index in range(cursor, len(self._history)):
                if predicate(self._history[index]):
                    return index + 1
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"timed out after {timeout:.3f}s waiting for {description}")
            async with self._history_changed:
                try:
                    await asyncio.wait_for(self._history_changed.wait(), remaining)
                except asyncio.TimeoutError as error:
                    raise TimeoutError(
                        f"timed out after {timeout:.3f}s waiting for {description}"
                    ) from error


def _parse_int(value: Any) -> int | None:
    if value is None:
        return None
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def _monotonic_ms() -> int:
    return int(time.monotonic() * 1000)


def _classify_sa_output(
    text: str,
) -> tuple[str, str, dict[str, Any], str] | None:
    if text == "=== SA build ===":
        return "sa", "sa.build.started", {}, "info"
    if (
        "supervisory_controller.vcxproj ->" in text
        or text.endswith("/supervisory_controller")
    ):
        return "sa", "sa.build.ready", {}, "info"
    if text == "=== SA runtime ===":
        return "sa", "sa.runtime.started", {}, "info"
    if text.startswith("Creating database session on url:"):
        endpoint = text.removeprefix("Creating database session on url:").strip()
        endpoint = endpoint.removeprefix("tcp://").removesuffix("...")
        return "sa", "sa.database.connecting", {"endpoint": endpoint}, "info"
    if text == "Database connection active.":
        return "sa", "sa.database.connected", {}, "info"
    if text.startswith("SA database startup failed:"):
        message = text.removeprefix("SA database startup failed:").strip()
        return "sa", "sa.database.failed", {"message": message}, "error"
    if text.startswith("SA exited with code"):
        return "sa", "sa.runtime.failed", {"message": text}, "error"
    return None


def _json_values(values: dict[str, Any]) -> dict[str, Any]:
    return {
        key: value.isoformat() if hasattr(value, "isoformat") else value
        for key, value in values.items()
    }
