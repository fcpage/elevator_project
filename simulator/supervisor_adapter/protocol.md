# Legacy Finite-Test JSON-Lines Protocol

NOTE:LEGACY. NOT MAINTAINED.

This document applies **only** to the legacy finite runner:

```powershell
cd simulator
python run_scenario.py basic_floor_request --supervisor-command <command>
```

It describes the small example program in this directory. It is not the
persistent two-terminal harness launched by `run.ps1`/`run.sh` and
`run-sa.ps1`/`run-sa.sh`. New branch integration should use the normal SA
`main.cpp` with `cRuntimeCanService`, as described in
[../MANUAL.md](../MANUAL.md) and
[../integration_kit/LEGACY_BRANCH_QUICKSTART.md](../integration_kit/LEGACY_BRANCH_QUICKSTART.md).

Every legacy-test message is one JSON object followed by a newline.

## Messages into the example supervisor

CAN frame received by the supervisor:

```json
{"type":"can_rx","id":513,"data":[3],"timestamp_ms":250}
```

Web/front-end request:

```json
{"type":"web_request","floor":3}
```

## Messages out of the example supervisor

CAN frame transmitted by the supervisor:

```json
{"type":"can_tx","id":256,"data":[7]}
```

Optional human-readable log line:

```json
{"type":"log","message":"dispatching floor 3"}
```

The legacy runner has no simulated-time `tick` message and no state `status`
message. Those fields were documented historically but are not emitted or
consumed by the current example.

## CAN constants shared by the simulator

- `0x100`: Supervisory Controller
- `0x101`: Elevator Controller
- `0x200`: Car Controller
- `0x201`: Floor 1 Controller
- `0x202`: Floor 2 Controller
- `0x203`: Floor 3 Controller

The shared one-byte protocol encodes a supervisor dispatch as floor bits 0–1
with bit 2 set. Floor 3 with enable set is `0x07`.

The simulator constants use a physical CAN bitrate of 250 kb/s. The legacy
JSON-lines process path does not emulate wire timing or bitrate.
