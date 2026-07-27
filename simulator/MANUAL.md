# Project 6 Elevator Simulator Manual

This is an integration fixture for the Supervisory Application (SA). It runs a
simulated FC1–FC3, CC, and EC around the real SA. It does not stress test the SA.

There are two supported operating modes:

| Mode | Use it when | Starts |
| --- | --- | --- |
| Full harness (default) | Testing the normal SA end-to-end | CAN plant, simulator-owned MariaDB, GUI journeys, trace, and optional diagnostics |
| Plant-only | Checking an application's CAN/hardware interaction | CAN plant only: heartbeats, doors, motion, and arrival reports |

The normal user workflow is always two terminals. After one-time setup, the
developer starts the simulator in Terminal 1 and the SA in Terminal 2. No one
chooses a database address, starts MariaDB manually, requires a simulator specific 
main file, or edits production configuration.

The default database contract is `agreed-v1`. A future compatible schema is a
new SQL file plus profile JSON, not a Python rewrite; see
[schema/README.md](schema/README.md).

## 1. One-time host setup

Start at the repository root. Ex: 
```aiignore
project 6\
    elevator_project\
    project_website\
    simulator\
```

### Windows

Install Python 3.11+, MariaDB Server, CMake, a C++20 compiler, and MySQL
Connector/C++. Then run:

```powershell
.\simulator\setup.ps1
```

`setup.ps1` creates the Python virtual environment and an isolated simulator
state directory under:

```text
%LOCALAPPDATA%\Project6ElevatorSimulator
```

### Linux

Install the platform prerequisites:

```bash
sudo apt update
sudo apt install -y \
  cmake build-essential mariadb-server mariadb-client \
  libmysqlcppconn-dev iproute2 can-utils
```

Python 3.11+ is required. Ubuntu 22.04 users must install it separately because
the default Python is 3.10:

```bash
sudo apt install -y software-properties-common
sudo add-apt-repository -y ppa:deadsnakes/ppa
sudo apt update
sudo apt install -y python3.11 python3.11-venv
```

Configure virtual CAN once per boot:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
ip link show vcan0
```

Then run:

```bash
./simulator/setup.sh
```

Linux state is stored outside the repository at:

```text
${XDG_STATE_HOME:-~/.local/state}/project6-elevator-simulator
```

## 2. First proof: physical elevator only

This is the smallest useful test. It needs no MariaDB and does not require a
testpoint or a GUI scenario. The plant replies to SA heartbeat requests within
50 ms, reports door status, takes two seconds per floor, and sends EC arrival
frames. The default healthy loop continuously injects its CAN-only physical
floor-call journey in this mode, so a connected SA should visibly dispatch
floor 3 and receive its arrival report.

Windows:

```powershell
.\simulator\run.ps1 -PlantOnly
```

Linux:

```bash
./simulator/run.sh --plant-only
```

`run-sa` may also launch a branch's normal executable in plant-only mode. It
passes only the CAN transport settings. Use this for branches that do not start
a database worker (for example, a branch using a temporary demo-control seam).
Branches whose normal main always starts a database worker should use the full
harness instead.

Press `Ctrl+C` to stop the plant.

To run one floor-3 request instead of the repeating healthy
loop:

```powershell
.\simulator\run.ps1 -PlantOnly -Scenario scenarios\basic_floor_request.json
```

```bash
./simulator/run.sh --plant-only --scenario scenarios/basic_floor_request.json
```
Warning: Scenarios are largely unmaintained. They're holdouts from early development
they may or may not work.

## 3. Full harness: the normal workflow

Open two terminals in the repository root.

Terminal 1 starts the simulator:

```powershell
.\simulator\run.ps1
```

```bash
./simulator/run.sh
```

Terminal 2 builds and starts the normal SA target:

```powershell
.\simulator\run-sa.ps1
```

```bash
./simulator/run-sa.sh
```

The launcher reads the active session created by Terminal 1. It supplies the
simulator CAN endpoint, the isolated database endpoint (`127.0.0.1:3307`),
credentials, and diagnostic endpoint and it builds `supervisory_controller`.

In healthy mode, the simulator attempts equivalent journeys in sequence:

1. Inserts a GUI floor-3 request into `guiRequests`, then waits for the SA.
2. Simulates a car selection for floor 1.
3. Simulates a physical floor-3 call, then another car selection for floor 1.

The simulator waits for observed commands and arrival/door behaviour; it does
not blindly advance after fixed sleeps. If the SA fails an expectation, new
rider injection pauses while plant heartbeats and diagnostics continue.

If a branch has no database worker, the full harness logs that the GUI journey
was skipped and continues the physical floor-call journey. Plant-only mode is
still the smallest and clearest acceptance test for that kind of branch.

### What to watch

- Terminal 1: concise plant, state, database, warning, and testpoint events.
- Terminal 2: direct SA startup/runtime output.
- `trace.ndjson`: full globally ordered trace for a run.
- `supervisory.log`: complete SA output for that run.

The active run directory is printed when the simulator starts. Raw frames and
routine polling stay in NDJSON unless `-VerboseTrace` (Windows) or `--verbose`
(Linux) is selected.

## 4. Adopting the harness on a legacy branch

An older branch may have a simulator-specific adapter executable or a second
`main()`. Do not keep that as the normal test path. The goal is to run the
branch's ordinary application entry point with a build-selected transport.

Use [`integration_kit/`](integration_kit/) as the canonical copy/paste bundle.
It contains the required `.hpp`/`.cpp` files, a CMake fragment, and commented
snippets that identify exactly where each integration line belongs.

For a strict first-time, zero-to-working sequence, follow
[integration_kit/LEGACY_BRANCH_QUICKSTART.md](integration_kit/LEGACY_BRANCH_QUICKSTART.md).

### Required simulator integration files

Bring these files from the simulator-integrated baseline into the legacy SA
tree, preserving their paths:

```text
SupervisoryController/include/supervisory/can/runtime_can_service.hpp
SupervisoryController/include/supervisory/sim/simulator_can_service.hpp
SupervisoryController/src/sim/simulator_can_service.cpp
```

Those three files are the minimum plant-only integration. The diagnostics and
testpoint files are optional add-ons for a branch that has the baseline
database exchange:

```text
SupervisoryController/include/supervisory/sim/simulator_diagnostics.hpp
SupervisoryController/src/sim/simulator_diagnostics.cpp
simulator/integration_kit/cpp/include/project6_sim/testpoints.hpp
simulator/integration_kit/cmake/simulator_integration.cmake
```

Do not bring the temporary database compatibility reference from
`C:\tmp\project6-db-thread-reference`; it is deliberately not part of the
simulator integration and does not belong on a branch unless the database team
chooses to solve that issue.

### Entry-point change

In the branch's existing `main.cpp`, replace direct construction of
`cCanCommsService` with the runtime-selected type:

```cpp
#include "supervisory/can/runtime_can_service.hpp"

// Keep the branch's existing configuration, exchange, and startup sequence.
cRuntimeCanService commsService(canConfig, canExchange);
```

`cRuntimeCanService` selects `cCanCommsService` in an ordinary build and the
localhost simulator bridge only when `SUPERVISORY_USE_SIMULATOR_CAN` is set.
Do not add a simulator `main()`.

### CMake changes

Add `src/sim/simulator_can_service.cpp` to the existing SA library target. Add
`src/sim/simulator_diagnostics.cpp` only on a branch with the baseline database
exchange. Then add these options and definitions using the same target names
used by the baseline:

```cmake
option(SUPERVISORY_BUILD_SIMULATOR "Use the localhost simulator CAN transport" OFF)
option(SUPERVISORY_ENABLE_SIM_DIAGNOSTICS "Enable simulator diagnostics" OFF)
option(SUPERVISORY_ENABLE_SIM_TESTPOINTS "Enable optional simulator testpoints" OFF)

if(SUPERVISORY_BUILD_SIMULATOR)
    target_compile_definitions(supervisory_controller
        PRIVATE SUPERVISORY_USE_SIMULATOR_CAN=1)
endif()

if(WIN32)
    target_link_libraries(project6_supervisory PUBLIC ws2_32)
endif()

if(SUPERVISORY_ENABLE_SIM_DIAGNOSTICS)
    target_compile_definitions(supervisory_controller
        PRIVATE SUPERVISORY_ENABLE_SIM_DIAGNOSTICS=1)
endif()

if(SUPERVISORY_ENABLE_SIM_TESTPOINTS)
    target_compile_definitions(project6_supervisory
        PRIVATE SUPERVISORY_ENABLE_SIM_TESTPOINTS=1)
    target_compile_definitions(supervisory_controller
        PRIVATE SUPERVISORY_ENABLE_SIM_TESTPOINTS=1)
endif()
```

The normal launcher already supplies these options. A branch that uses
different CMake target names should make the equivalent changes rather than
creating an adapter executable.

### Database endpoint override

The full harness owns `127.0.0.1:3307`. In simulator diagnostics mode only,
read these environment variables into the branch's existing database config:

```text
ELEVATOR_DB_URL
ELEVATOR_DB_USER
ELEVATOR_DB_PASSWORD
ELEVATOR_DB_SCHEMA
```

Outside simulator mode, leave the branch's production defaults unchanged. Do
not autodetect or reuse another database instance. If port 3307 is occupied,
the simulator stops rather than attaching to an unknown database.

## 5. Adding testpoints

Testpoints are optional breadcrumbs for multi-threaded investigation. A branch
with zero testpoints works normally. The current diagnostics implementation
captures the baseline database exchange; do not copy it to a no-database branch
solely to gain testpoints. Plant-only CAN testing remains supported there.

In a source file you own:

```cpp
#include "project6_sim/testpoints.hpp"

PROJECT6_SIM_TESTPOINT(
    "database.row.claimed",
    "guiRequests row accepted by database worker");
```

The macro is a no-op when simulator testpoints are disabled; its arguments are
not evaluated. When enabled, it puts a bounded record onto a best-effort worker
queue. A slow or disconnected simulator drops diagnostics rather than blocking
CONTROL or a database/CAN worker.

Good locations:

- before and after claiming a database row;
- immediately before a worker pushes to a shared queue;
- immediately after CONTROL pops an event;
- at a recovery/fault branch;
- at an otherwise ambiguous state transition.

Avoid a testpoint every control-loop cycle.

Use concise stable names: `database.row.claimed`, `can.command.enqueued`, or
`control.transition.arrived`. The terminal prints the name and detail; the
NDJSON record also has a per-process sequence and a thread identifier.

## 6. Scenarios and faults

The default scenario is healthy and repeats indefinitely. Named deterministic
fault scenarios are available under `simulator/scenarios/`; select one with:

```powershell
.\simulator\run.ps1 -Scenario scenarios\fault_heartbeat_fc2.json
```

```bash
./simulator/run.sh --scenario scenarios/fault_heartbeat_fc2.json
```

Fault scenarios inject one selected fault, then preserve observation. They are
for reproducing a known issue—not load testing, random failure generation, or
stress testing.

## 7. Selecting or replacing a schema profile

The default is the agreed `agreed-v1` profile. To use a future schema, add its
SQL and `*.profile.json` mapping under `simulator/schema/`, then select it in
both setup and run:

```powershell
.\simulator\setup.ps1 -SchemaProfile future-v2 -RecreateSchema
.\simulator\run.ps1 -SchemaProfile future-v2
```

```bash
./simulator/setup.sh --schema-profile future-v2 --recreate-schema
./simulator/run.sh --schema-profile future-v2
```

`-RecreateSchema` / `--recreate-schema` is intentional: it drops and rebuilds
only the selected schema in the simulator-owned MariaDB. Normal setup never
tries to infer or apply a table migration. The profile file defines the queue
table names, columns, reset targets, GUI insert mapping, and observation cursor;
the Python engine stays unchanged. Follow the exact profile format in
[schema/README.md](schema/README.md).

## 8. Troubleshooting

| Symptom | Meaning and next action |
| --- | --- |
| `No active simulator session` | Start `run` in Terminal 1 first. |
| Port 3307 is occupied | Stop the process using that port or choose a machine without a conflicting simulator. The harness will not reuse an unknown server. |
| `Unknown column 'requestedFloor'` | This is an observed legacy SA/schema mismatch. It is not fixed by the simulator; capture the trace and hand it to the database owners. |
| Linux cannot create `vcan0` | Update WSL first. If its kernel still lacks `vcan`, use native Linux/VM for SocketCAN or the Windows loopback path. Do not make a custom WSL kernel the required team workflow. |
| Python 3.10 rejected on Ubuntu 22.04 | Install Python 3.11 and `python3.11-venv`, remove only the simulator `venv`, then rerun `setup.sh`. |
| MySQL Connector/C++ not found | Install `libmysqlcppconn-dev` on Linux. On Windows install MySQL Connector/C++; the launcher searches normal Program Files locations. |
| No testpoints appear | Confirm the normal full harness is running, include `project6_sim/testpoints.hpp`, and build with `SUPERVISORY_ENABLE_SIM_TESTPOINTS=ON`. |
| A new schema profile fails validation | Check its ordered `columns`, `index_column`, queue roles, and GUI field mapping. If the old simulator schema still exists under the same name, rerun setup with `--recreate-schema`. |

## 9. Before handing a branch to another developer

1. Run the two-terminal healthy journey once: plant-only for a CAN-only branch,
   or the full harness for a database-aware branch.
2. Confirm the branch uses its ordinary `main()` and has no required simulator
   adapter executable.
3. Confirm a production build has no simulator compile definitions.
4. If testpoints were added, keep them short and verify they compile to no-ops
   in a non-simulator build.
5. Share the repository only. Do not share generated databases, active-session
   files, logs, traces, virtual environments, or the external database
   compatibility reference.
