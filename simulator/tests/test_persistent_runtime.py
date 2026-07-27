import asyncio
import contextlib
import io
import json
import socket
import tempfile
import unittest
from datetime import datetime
from pathlib import Path

from sim.can import CanFrame
from sim.cli import build_parser
from sim.engine import PersistentSimulator, _classify_sa_output
from sim.mariadb_runtime import _split_sql
from sim.protocol import (
    EC_CAN_ID,
    FLOOR_CAN_IDS,
    HB_OK,
    HB_SC_REQUEST,
    SC_CAN_ID,
    describe_frame,
)
from sim.scenario import load_scenario
from sim.schema_profile import load_schema_profile
from sim.trace import TraceEvent, TraceRecorder, _live_message
from sim.transport import LoopbackCanTransport


SIMULATOR_ROOT = Path(__file__).resolve().parent.parent


class ScenarioTests(unittest.TestCase):
    def test_plant_only_is_an_explicit_runtime_mode(self):
        arguments = build_parser().parse_args(["run", "--plant-only"])

        self.assertTrue(arguments.plant_only)

    def test_schema_profile_is_selectable_for_setup_and_run(self):
        setup = build_parser().parse_args(
            ["setup", "--schema-profile", "future-v2", "--recreate-schema"]
        )
        run = build_parser().parse_args(["run", "--schema-profile", "future-v2"])

        self.assertEqual(setup.schema_profile, "future-v2")
        self.assertTrue(setup.recreate_schema)
        self.assertEqual(run.schema_profile, "future-v2")

    def test_legacy_basic_scenario_is_normalized_to_refit_actions(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "legacy.json"
            path.write_text(
                json.dumps(
                    {
                        "version": 1,
                        "name": "legacy",
                        "steps": [
                            {"type": "floor_call", "floor": 3},
                            {"type": "advance", "ms": 1},
                            {"type": "expect_floor", "floor": 3},
                        ],
                    }
                ),
                encoding="utf-8",
            )
            scenario = load_scenario(path)

        self.assertEqual(
            [step.action for step in scenario.steps],
            ["floor_call", "wait", "wait_for_can"],
        )
        self.assertEqual(scenario.steps[-1].arguments["id"], "0x101")
        self.assertEqual(scenario.steps[-1].arguments["data_byte"], 3)

    def test_healthy_scenario_is_versioned_and_repeating(self):
        scenario = load_scenario(SIMULATOR_ROOT / "scenarios" / "healthy_loop.json")

        self.assertEqual(scenario.version, 1)
        self.assertTrue(scenario.repeat)
        self.assertEqual(len(scenario.journeys), 2)
        self.assertEqual(scenario.steps[0].action, "gui_request")
        self.assertIn("floor_call", [step.action for step in scenario.steps])

    def test_fault_scenarios_are_deterministic_one_shots(self):
        for path in sorted((SIMULATOR_ROOT / "scenarios").glob("fault_*.json")):
            with self.subTest(path=path.name):
                scenario = load_scenario(path)
                self.assertFalse(scenario.repeat)


class TraceTests(unittest.TestCase):
    def test_trace_records_versioned_ndjson_with_monotonic_sequence(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "trace.ndjson"
            recorder = TraceRecorder("test-run", path)
            recorder.emit("test", "first", {"value": 1}, live=False)
            recorder.emit("test", "second", {"value": 2}, live=False)
            recorder.close()

            lines = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
            self.assertEqual([line["sequence"] for line in lines], [1, 2])
            self.assertTrue(all(line["version"] == 1 for line in lines))
            self.assertTrue(all(line["run_id"] == "test-run" for line in lines))

    def test_sa_output_promotes_only_meaningful_live_events(self):
        self.assertIsNone(
            _classify_sa_output(
                r"database_message_service.cpp(210): warning C4244: possible loss of data"
            )
        )

        event = _classify_sa_output(
            "SA database startup failed: Unknown column 'requestedFloor' in 'SET'"
        )

        self.assertIsNotNone(event)
        source, kind, payload, severity = event
        self.assertEqual((source, kind, severity), ("sa", "sa.database.failed", "error"))
        self.assertIn("requestedFloor", payload["message"])

    def test_raw_process_output_requires_verbose_trace(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "trace.ndjson"
            normal = TraceRecorder("normal", path)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                normal.emit("sa-log", "process.output", {"line": "compiler detail"})
            normal.close()
            self.assertEqual(output.getvalue(), "")

            verbose = TraceRecorder("verbose", path, verbose=True)
            with contextlib.redirect_stdout(output):
                verbose.emit("sa-log", "process.output", {"line": "compiler detail"})
            verbose.close()
            self.assertIn("compiler detail", output.getvalue())

    def test_testpoint_live_output_identifies_name_detail_and_thread(self):
        event = TraceEvent(
            version=1,
            run_id="test",
            sequence=1,
            wall_time="2026-07-25T00:00:00Z",
            monotonic_ms=1,
            source="sa",
            kind="testpoint.hit",
            severity="info",
            payload={
                "name": "database.row.claimed",
                "detail": "row=17",
                "thread_id": 42,
            },
        )

        label, message, continuation = _live_message(event)

        self.assertEqual((label, message), ("TEST", "database.row.claimed"))
        self.assertEqual(continuation, "row=17 (thread 42)")


class FakeCursor:
    def __init__(self):
        self.executed = []
        self.rows = []

    def execute(self, operation, params=None):
        self.executed.append((operation, params))

    def fetchall(self):
        return list(self.rows)


class SchemaProfileTests(unittest.TestCase):
    def setUp(self):
        self.profile = load_schema_profile(SIMULATOR_ROOT / "schema", "agreed-v1")

    def test_profile_manifest_can_change_request_column_without_python_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "next.sql").write_text("CREATE SCHEMA next_schema;", encoding="utf-8")
            (directory / "next.profile.json").write_text(
                json.dumps(
                    {
                        "version": 1,
                        "name": "next-v2",
                        "schema_name": "next_schema",
                        "sql_file": "next.sql",
                        "tables": {
                            "requests": {
                                "name": "requests",
                                "index_column": "row_id",
                                "columns": ["row_id", "requested_floor", "mode"],
                                "reset": True,
                            },
                            "state": {
                                "name": "state_rows",
                                "index_column": "row_id",
                                "columns": ["row_id", "floor"],
                                "reset": True,
                            },
                        },
                        "gui_request": {
                            "table": "requests",
                            "fields": [
                                {"column": "requested_floor", "source": "floor"},
                                {"column": "mode", "source": "remote"},
                            ],
                        },
                        "outbound_table": "state",
                    }
                ),
                encoding="utf-8",
            )
            profile = load_schema_profile(directory, "next-v2")
            cursor = FakeCursor()
            profile.insert_gui_request(cursor, floor=2, remote=9)

            self.assertIn("`requested_floor`", cursor.executed[-1][0])
            self.assertEqual(cursor.executed[-1][1], (2, 9))
            self.assertEqual(profile.inbound_table, "requests")

    def test_gui_insert_uses_exact_agreed_column_name(self):
        cursor = FakeCursor()

        self.profile.insert_gui_request(
            cursor,
            floor=3,
            remote=0,
            timestamp=datetime(2026, 7, 24, 12, 30, 15),
        )

        query, params = cursor.executed[-1]
        self.assertIn("`requestFloor`", query)
        self.assertNotIn("requestedFloor", query)
        self.assertEqual(params[2:], (3, 0))

    def test_reset_only_targets_agreed_simulator_tables(self):
        cursor = FakeCursor()

        self.profile.reset(cursor)

        queries = "\n".join(query for query, _ in cursor.executed)
        self.assertIn("`guiRequests`", queries)
        self.assertIn("`elevatorNetwork`", queries)
        self.assertNotIn("DROP", queries.upper())
        self.assertNotIn("ALTER", queries.upper())

    def test_schema_source_retains_agreed_time_unique_key(self):
        sql = self.profile.schema_sql()

        self.assertIn("ADD UNIQUE KEY(`time`)", sql)
        self.assertIn("CREATE TABLE guiRequests", sql)
        statements = _split_sql(sql)
        self.assertTrue(any("CREATE TABLE guiRequests" in statement for statement in statements))
        self.assertEqual(statements[-2:], ["DESC elevatorNetwork", "DESC guiRequests"])

    def test_validation_accepts_windows_lowercase_table_metadata(self):
        rows = []
        for table, columns in self.profile.required_columns.items():
            rows.extend((table.lower(), column) for column in columns)
        cursor = FakeCursor()
        cursor.rows = rows

        self.profile.validate(cursor)


class ProtocolDescriptionTests(unittest.TestCase):
    def test_heartbeat_request_is_decoded_for_trace(self):
        frame = CanFrame(can_id=SC_CAN_ID, data=[HB_SC_REQUEST])

        self.assertEqual(describe_frame(frame), "supervisor_heartbeat_request")


class LoopbackTransportTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reservation:
            reservation.bind(("127.0.0.1", 0))
            self.port = reservation.getsockname()[1]
        self.transport = LoopbackCanTransport("127.0.0.1", self.port)
        await self.transport.start()

    async def asyncTearDown(self):
        await self.transport.stop()

    async def test_fragmented_and_multiple_lines_are_read_independently(self):
        reader, writer = await asyncio.open_connection("127.0.0.1", self.port)
        writer.write(b'{"version":1,"type":"hel')
        await writer.drain()
        writer.write(
            b'lo"}\n'
            b'{"version":1,"type":"can_tx","id":256,"data":[7]}\n'
            b'{"version":1,"type":"diagnostic","control":{"state":"MovingUp"}}\n'
        )
        await writer.drain()

        acknowledgement = json.loads(await reader.readline())
        self.assertEqual(acknowledgement["type"], "hello_ack")
        connected = await asyncio.wait_for(self.transport.events.get(), 1)
        can_event = await asyncio.wait_for(self.transport.events.get(), 1)
        diagnostic_event = await asyncio.wait_for(self.transport.events.get(), 1)
        self.assertEqual(connected.kind, "transport.connected")
        self.assertEqual(can_event.kind, "can.tx")
        self.assertEqual(diagnostic_event.kind, "diagnostic")

        writer.close()
        await writer.wait_closed()


class FakeTransport:
    def __init__(self):
        self.sent = []

    async def send(self, frame):
        self.sent.append(frame)


class PlantBehaviorTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.trace = TraceRecorder(
            "plant-test", Path(self.temporary.name) / "trace.ndjson"
        )
        self.transport = FakeTransport()
        scenario = load_scenario(SIMULATOR_ROOT / "scenarios" / "healthy_loop.json")
        self.simulator = PersistentSimulator(
            transport=self.transport,
            diagnostics=object(),
            database=object(),
            database_observer=object(),
            scenario=scenario,
            trace=self.trace,
            sa_log_path=Path(self.temporary.name) / "sa.log",
            heartbeat_delay_ms=1,
            travel_ms_per_floor=1,
        )

    async def asyncTearDown(self):
        if self.simulator._movement_task is not None:
            await asyncio.gather(self.simulator._movement_task, return_exceptions=True)
        self.trace.close()
        self.temporary.cleanup()

    async def test_healthy_heartbeat_replies_include_all_required_nodes(self):
        await self.simulator._handle_supervisor_frame(
            CanFrame(can_id=SC_CAN_ID, data=[HB_SC_REQUEST])
        )

        self.assertEqual(len(self.transport.sent), 4)
        self.assertTrue(all(frame.data == [HB_OK] for frame in self.transport.sent))
        self.assertIn(FLOOR_CAN_IDS[2], [frame.can_id for frame in self.transport.sent])

    async def test_plant_only_keeps_hardware_behavior_without_database_or_diagnostics(self):
        self.simulator.plant_only = True
        self.simulator.database = None
        self.simulator.database_observer = None
        self.simulator.diagnostics = None

        await self.simulator._handle_supervisor_frame(
            CanFrame(can_id=SC_CAN_ID, data=[HB_SC_REQUEST])
        )

        self.assertEqual(len(self.transport.sent), 4)

    async def test_can_readiness_allows_physical_journeys_without_database_readiness(self):
        self.simulator._sa_transport_ready.set()

        await asyncio.wait_for(
            self.simulator._wait_for_sa_readiness(require_database=False),
            timeout=1,
        )

    async def test_dispatch_produces_moving_and_arrival_status(self):
        await self.simulator._handle_supervisor_frame(
            CanFrame(can_id=SC_CAN_ID, data=[0x07])
        )
        await asyncio.wait_for(self.simulator._movement_task, 1)

        elevator_frames = [frame for frame in self.transport.sent if frame.can_id == EC_CAN_ID]
        self.assertEqual(elevator_frames[0].data, [0x05])
        self.assertEqual(elevator_frames[-1].data, [3])
        self.assertEqual(self.simulator.current_floor, 3)

    async def test_heartbeat_fault_suppresses_only_one_cycle(self):
        self.simulator.faults.suppressed_heartbeat_node = FLOOR_CAN_IDS[2]
        self.simulator.faults.suppressed_heartbeat_cycles = 1

        await self.simulator._handle_supervisor_frame(
            CanFrame(can_id=SC_CAN_ID, data=[HB_SC_REQUEST])
        )
        first_cycle_ids = [frame.can_id for frame in self.transport.sent]
        self.transport.sent.clear()
        await self.simulator._handle_supervisor_frame(
            CanFrame(can_id=SC_CAN_ID, data=[HB_SC_REQUEST])
        )
        second_cycle_ids = [frame.can_id for frame in self.transport.sent]

        self.assertNotIn(FLOOR_CAN_IDS[2], first_cycle_ids)
        self.assertIn(FLOOR_CAN_IDS[2], second_cycle_ids)

    async def test_payload_selected_drop_does_not_consume_heartbeat(self):
        self.simulator.faults.drop_direction = "sa_to_sim"
        self.simulator.faults.drop_can_id = SC_CAN_ID
        self.simulator.faults.drop_data_byte = 0x07

        self.assertFalse(
            self.simulator._should_drop(
                "sa_to_sim", CanFrame(can_id=SC_CAN_ID, data=[HB_SC_REQUEST])
            )
        )
        self.assertTrue(
            self.simulator._should_drop(
                "sa_to_sim", CanFrame(can_id=SC_CAN_ID, data=[0x07])
            )
        )


if __name__ == "__main__":
    unittest.main()
