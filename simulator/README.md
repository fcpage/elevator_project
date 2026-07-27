# Project 6 Persistent Elevator Simulator

For a first-time developer or a legacy-branch migration, start with the
[Simulator Manual](MANUAL.md).

The simulator runs a healthy elevator plant around the real supervisory
application and records CAN, MariaDB, process, and diagnostic activity in one
timeline. It stays active until stopped.

The agreed database schema is stored verbatim in `schema/agreed_v1.sql`.
Simulator runtime data is never written into the repository.

Future schemas are selected through a versioned profile rather than Python
edits. See [schema/README.md](schema/README.md). The default profile is
`agreed-v1`.

## General use

After setup, the full integration harness is two commands:

```powershell
# Terminal 1
.\simulator\run.ps1

# Terminal 2
.\simulator\run-sa.ps1
```

```bash
# Terminal 1
./simulator/run.sh

# Terminal 2
./simulator/run-sa.sh
```

The simulator always owns its MariaDB data directory and `127.0.0.1:3307`.
If that port is occupied it stops with an error; it never attaches to an
unknown server or resets a developer's existing database.

The healthy loop never waits indefinitely for database readiness before
exercising the physical plant. If an SA branch has no database worker, its GUI
journey is reported as skipped while the physical floor-call journey continues
to dispatch the car and produce arrival frames.

### Plant-only mode

Use plant-only mode when the only requirement is a responsive physical elevator
environment:

```powershell
.\simulator\run.ps1 -PlantOnly
```

```bash
./simulator/run.sh --plant-only
```

This needs no MariaDB and provides FC/CC/EC heartbeats, door frames, and real
arrival reports at two seconds per floor. It does not inject GUI requests,
start a database, or collect diagnostics. It does run CAN-only scenario
journeys: the healthy loop continuously uses its physical floor-call journey,
so the SA should visibly dispatch floors and receive EC arrivals. See [INTEGRATION.md](INTEGRATION.md)
for the optional testpoint header and branch integration seam.

## First-Time Setup

System prerequisites are Python 3.11+, MariaDB server/client binaries, CMake, a
C++20 compiler, and MySQL Connector/C++. The current setup preflight checks all
of these even when you intend to use plant-only mode; a database-aware SA also
needs Connector/C++ at build/runtime. The setup scripts install Python packages
into a user-state virtual environment and give exact platform guidance when a
system prerequisite is absent.

Ubuntu 22.04 ships Python 3.10. Install Python 3.11 and its venv package before
running setup; the script automatically selects it without changing the system
`python3` default:

```bash
sudo add-apt-repository ppa:deadsnakes/ppa
sudo apt update
sudo apt install python3.11 python3.11-venv
```

Windows PowerShell:

```powershell
.\simulator\setup.ps1
```

Linux:

```bash
./simulator/setup.sh
```

Setup creates an isolated MariaDB instance under the user state directory. It
listens only on `127.0.0.1:3307` and does not attach to an existing database.

## Normal Two-Terminal Workflow

Terminal 1 starts the simulator:

```powershell
.\simulator\run.ps1
```

```bash
./simulator/run.sh
```

Terminal 2 builds incrementally and starts the SA:

```powershell
.\simulator\run-sa.ps1
```

`run-sa.ps1` keeps compiler details in the active run log and shows concise
build status by default. Use `-VerboseBuild` when the complete compiler output
is useful in the terminal:

```powershell
.\simulator\run-sa.ps1 -VerboseBuild
```

```bash
./simulator/run-sa.sh
```

Both platforms build and execute the ordinary `supervisory_controller` target
and its existing `main.cpp`. Windows selects a localhost CAN transport at build
time; Linux selects the normal SocketCAN service on `vcan0`. No second
simulator-specific entry point is required.

The healthy scenario alternates a GUI request and a physical floor-controller
call. Heartbeats always receive timely replies and the simulated elevator
always reports arrival. If the SA misses an expected step, rider injection
pauses while observation continues.

Press Ctrl+C in each terminal to stop.

## Deterministic Fault Scenarios

Faults are opt-in JSON scenarios. For example:

```powershell
.\simulator\run.ps1 -Scenario scenarios\fault_heartbeat_fc2.json
```

```bash
./simulator/run.sh scenarios/fault_heartbeat_fc2.json
```

Included scenarios cover:

- one missing FC2 heartbeat reply;
- one arrival delayed beyond the SA timeout;
- a two-second outage of the simulator-owned database; and
- one dropped supervisor dispatch frame.

Scenarios use versioned actions: `gui_request`, `floor_call`, `car_request`,
`wait_for_can`, `wait_for_database`, `wait`, `suppress_heartbeat`,
`delay_arrival`, `database_outage`, and `drop_next_frame`.

## Output

Normal Terminal 1 output uses short component labels such as `SIM`, `LINK`,
`SA`, `SA/DB`, `READY`, and `WAIT`. Compiler output is not repeated there;
meaningful build/runtime milestones and failures are promoted into concise
events. Data-heavy events place their details on an indented second line.

The complete compiler/SA output remains visible in Terminal 2 and is saved in
the active run's `supervisory.log`. Every event, including raw process output,
is also retained in `trace.ndjson`. Runtime data is stored under:

- Windows: `%LOCALAPPDATA%\Project6ElevatorSimulator`
- Linux: `${XDG_STATE_HOME:-~/.local/state}/project6-elevator-simulator`

Use `-VerboseTrace` on Windows or `--verbose` with `python -m sim run` to print
routine heartbeat, SQL polling, and unchanged diagnostic records.

Linux offers the matching launcher flags:

```bash
./simulator/run.sh --verbose
./simulator/run-sa.sh --verbose-build
```

For the build-selection seam, branch adoption checklist, simulator-only
`PROJECT6_SIM_TESTPOINT` macro, and debugging workflow, see
[INTEGRATION.md](INTEGRATION.md).

## Compatibility Tests

The original finite self-test remains available:

```powershell
cd simulator
python run_scenario.py basic_floor_request
```

Run all Python tests from `simulator/`:

```powershell
python -m unittest discover -s tests -v
```
