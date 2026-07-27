# Legacy Branch: Zero-to-Working Simulator Setup

Use this guide when you have checked out a branch that has never used the new
simulator. Follow the steps in order. Do not skip ahead to CMake: first copy
the adapter files, then change the existing `main.cpp`, then wire CMake.

This guide makes the branch work with the **physical elevator plant**. That
means simulated CAN nodes, heartbeat replies, doors, elevator movement, and
arrival frames. It does not change the branch's state machine, database logic,
audio logic, operating modes, or GUI logic.

## Before you start

You need:

1. A checkout containing the `simulator/` folder and the SA branch you want to
   test.
2. Windows or Linux prerequisites installed as described in
   [`../MANUAL.md`](../MANUAL.md#1-one-time-host-setup).
3. A clean or intentionally saved branch. The following steps modify only the
   SA branch's adapter/CMake integration files.

## Step 1: Set up the simulator once

Run this from the repository root.

Windows:

```powershell
.\simulator\setup.ps1
```

Linux:

```bash
./simulator/setup.sh
```

On Linux, create `vcan0` once per boot before running the simulator:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

The Linux launcher checks this interface again. It does not request `sudo` when
`vcan0` already exists and is up. It requests `sudo` only when it must create
or bring up the interface.

Do not start MariaDB manually. The full harness owns its own MariaDB instance;
plant-only mode needs no MariaDB at all.

## Step 2: Copy the three required adapter files

These are the minimum files for physical elevator simulation. Copy them from
the integration kit into the branch's `SupervisoryController` folder, retaining
the relative paths.

```text
FROM simulator/integration_kit/cpp/include/supervisory/can/runtime_can_service.hpp
TO   elevator_project/SupervisoryController/include/supervisory/can/runtime_can_service.hpp

FROM simulator/integration_kit/cpp/include/supervisory/sim/simulator_can_service.hpp
TO   elevator_project/SupervisoryController/include/supervisory/sim/simulator_can_service.hpp

FROM simulator/integration_kit/cpp/src/sim/simulator_can_service.cpp
TO   elevator_project/SupervisoryController/src/sim/simulator_can_service.cpp
```

Windows PowerShell copy commands:

```powershell
$kit = '.\simulator\integration_kit\cpp'
$sa = '.\elevator_project\SupervisoryController'
New-Item -ItemType Directory -Force "$sa\include\supervisory\can", "$sa\include\supervisory\sim", "$sa\src\sim"
Copy-Item "$kit\include\supervisory\can\runtime_can_service.hpp" "$sa\include\supervisory\can\runtime_can_service.hpp" -Force
Copy-Item "$kit\include\supervisory\sim\simulator_can_service.hpp" "$sa\include\supervisory\sim\simulator_can_service.hpp" -Force
Copy-Item "$kit\src\sim\simulator_can_service.cpp" "$sa\src\sim\simulator_can_service.cpp" -Force
```

Linux copy commands:

```bash
kit=./simulator/integration_kit/cpp
sa=./elevator_project/SupervisoryController
mkdir -p "$sa/include/supervisory/can" "$sa/include/supervisory/sim" "$sa/src/sim"
cp "$kit/include/supervisory/can/runtime_can_service.hpp" "$sa/include/supervisory/can/"
cp "$kit/include/supervisory/sim/simulator_can_service.hpp" "$sa/include/supervisory/sim/"
cp "$kit/src/sim/simulator_can_service.cpp" "$sa/src/sim/"
```

## Step 3: Change the branch's existing `main.cpp`

Open:

```text
elevator_project/SupervisoryController/src/main.cpp
```

Find the existing direct CAN service include, normally:

```cpp
#include "supervisory/can/can_comms_service.hpp"
```

Replace that one include with:

```cpp
// Uses production SocketCAN normally and the simulator bridge only for a
// simulator build. This does not add a second main().
#include "supervisory/can/runtime_can_service.hpp"
```

Then find the line that constructs the CAN service, normally:

```cpp
cCanCommsService commsService(canConfig, canExchange);
```

Replace it with:

```cpp
// Keep the existing config and exchange. The build chooses the transport.
cRuntimeCanService commsService(canConfig, canExchange);
```

Do not change anything else in `main.cpp`: preserve the branch's application,
audio, mode, database, and startup code.

## Step 4: Wire the copied source into CMake

Only after Steps 2 and 3 are complete, open:

```text
elevator_project/SupervisoryController/CMakeLists.txt
```

### 4a. Add the option

Place this with the other `option(...)` declarations near the top of the file:

```cmake
# Builds the normal SA executable with the localhost simulator CAN transport.
option(SUPERVISORY_BUILD_SIMULATOR
    "Build supervisory_controller with the localhost simulator CAN transport."
    OFF)
```

### 4b. Add the copied source file

Find the existing `add_library(project6_supervisory ... )` list. Add this line
inside that list:

```cmake
    src/sim/simulator_can_service.cpp
```

### 4c. Add the Windows socket dependency

Place this after the existing `target_link_libraries(project6_supervisory ...)`
block:

```cmake
if(WIN32)
    # Required by the localhost simulator CAN bridge on Windows.
    target_link_libraries(project6_supervisory PUBLIC ws2_32)
endif()
```

### 4d. Select the simulator transport for the existing executable

Place this after the existing `target_link_libraries(supervisory_controller ...)`
block:

```cmake
if(SUPERVISORY_BUILD_SIMULATOR)
    # This affects only simulator builds of the normal executable.
    target_compile_definitions(supervisory_controller
        PRIVATE SUPERVISORY_USE_SIMULATOR_CAN=1)
endif()
```

That completes the minimum branch integration.

### 4e. Database-aware branches: mirror diagnostics in Linux `rpi/CMakeLists.txt`

`run-sa.sh` configures `SupervisoryController/rpi/CMakeLists.txt`, not the
Windows root `CMakeLists.txt`. A database-aware branch that wants full-harness
diagnostics/testpoints must add the same two options there:

```cmake
option(SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
    "Enable best-effort localhost simulator diagnostics." OFF)
option(SUPERVISORY_ENABLE_SIM_TESTPOINTS
    "Enable optional, non-blocking simulator testpoint calls." OFF)
```

Then add the diagnostics implementation only when either option is enabled:

```cmake
if(SUPERVISORY_ENABLE_SIM_DIAGNOSTICS OR SUPERVISORY_ENABLE_SIM_TESTPOINTS)
    target_sources(supervisory_controller PRIVATE
        "${SUPERVISORY_ROOT}/src/sim/simulator_diagnostics.cpp")
endif()
```

Finally, add the corresponding definitions to the Linux executable target:

```cmake
if(SUPERVISORY_ENABLE_SIM_DIAGNOSTICS)
    target_compile_definitions(supervisory_controller PRIVATE
        SUPERVISORY_ENABLE_SIM_DIAGNOSTICS=1)
endif()

if(SUPERVISORY_ENABLE_SIM_TESTPOINTS)
    target_compile_definitions(supervisory_controller PRIVATE
        SUPERVISORY_ENABLE_SIM_TESTPOINTS=1)
endif()
```

The normal Linux path uses SocketCAN on `vcan0`; do not add the Windows
`SUPERVISORY_BUILD_SIMULATOR`/localhost CAN bridge to `rpi/CMakeLists.txt`.

## Step 5: Build once before launching

Windows:

```powershell
cmake -S .\elevator_project\SupervisoryController `
  -B "$env:LOCALAPPDATA\Project6ElevatorSimulator\build\branch-check" `
  -DSUPERVISORY_BUILD_SIMULATOR=ON
cmake --build "$env:LOCALAPPDATA\Project6ElevatorSimulator\build\branch-check" `
  --config Release --target supervisory_controller
```

Linux branches using the normal SocketCAN path should use their existing Linux
SA CMake entry point and enable virtual CAN according to that branch's build
configuration. Do not define `SUPERVISORY_BUILD_SIMULATOR` on Linux merely to
make the build pass: Linux normally uses real `vcan0`, not the Windows
localhost bridge.

For a database-aware Linux branch, verify that its Linux target also includes
`src/database/database_message_service.cpp` and links `mysqlcppconn`. If a
build reports `mysql_driver.h: No such file or directory`, install
`libmysqlcppconn-dev` and add its include/library settings to the target; this
is a database build dependency, not a simulator adapter problem.

## Step 6: Run the physical-plant acceptance test

Open two terminals at the repository root.

Terminal 1:

```powershell
.\simulator\run.ps1 -PlantOnly
```

```bash
./simulator/run.sh --plant-only
```

Terminal 2:

```powershell
.\simulator\run-sa.ps1
```

```bash
./simulator/run-sa.sh
```

Expected result:

1. The launcher builds the branch's normal `supervisory_controller` target.
2. The SA connects to the simulator CAN transport.
3. Terminal 1 reports `transport.connected` in its trace.
4. The healthy loop sends physical floor calls; a responsive SA dispatches
   floor 3 and the plant returns an EC arrival report.
5. The SA continues running until `Ctrl+C` is pressed.

This is the first success criterion. It proves that heartbeat and elevator
hardware simulation can surround the branch without a second main or custom
application code.

## Step 7: Decide whether this branch can use the full database harness

Use the full harness only when the branch has a real database worker that can
read the agreed schema and accepts the simulator database environment values.

If it does, start:

```powershell
.\simulator\run.ps1
```

then:

```powershell
.\simulator\run-sa.ps1
```

On Linux use `run.sh` and `run-sa.sh` instead. The simulator owns
`127.0.0.1:3307`; do not point it at another MariaDB server.

If the branch has no database worker or uses a temporary GUI/demo seam,
plant-only at Step 6 is the required acceptance test. The full harness may
still be used to observe the physical journey: it logs the unsupported GUI
journey as skipped and continues the CAN-only journey. Do not copy the database
diagnostics files to force database support onto a branch that does not have the
required database exchange.

For a database-aware branch, `main.cpp` must apply the four environment values
only in simulator diagnostics mode before constructing `cDBMessageService`:
`ELEVATOR_DB_URL`, `ELEVATOR_DB_USER`, `ELEVATOR_DB_PASSWORD`, and
`ELEVATOR_DB_SCHEMA`. This keeps production defaults unchanged while allowing
`run-sa` to select the isolated simulator database.

## Step 8: Optional testpoints (database-aware branches only)

Only after the physical plant works and only on a branch with the baseline
database exchange, copy the optional diagnostics files listed in
[`README.md`](README.md). Then add the corresponding diagnostics source and
definitions from [`cmake/simulator_integration.cmake`](cmake/simulator_integration.cmake).

Copy `project6_sim/testpoints.hpp` to the branch's include directory and add a
testpoint only where it clarifies an ordering question:

```cpp
#include "project6_sim/testpoints.hpp"

// Place at a queue/database/state boundary, never in every loop iteration.
PROJECT6_SIM_TESTPOINT("database.row.claimed", "row accepted by DB worker");
```

Testpoints are optional. They are not required to run the simulator.

### Full-harness acceptance evidence

On a working database branch, Terminal 1 / `trace.ndjson` should show this
order for a GUI journey:

```text
database.request_inserted
database.gui_request.decoded
database.event.enqueued
database.snapshot.written
scenario.journey.completed
```

The middle three are recommended database testpoints. If the GUI insert appears
but the first database testpoint does not, check the database schema mapping
and that both Linux diagnostics options in Step 4e are enabled.

## If something fails

| Problem | Check first |
| --- | --- |
| `runtime_can_service.hpp` not found | Step 2 paths were not preserved. |
| Link error mentioning Winsock | Step 4c (`ws2_32`) is missing on Windows. |
| `cRuntimeCanService` unknown | Step 3 include was not replaced or the file was not copied. |
| Simulator never sees a connection | Confirm Step 4d defines `SUPERVISORY_USE_SIMULATOR_CAN` in the executable target. |
| Linux has no `vcan0` | Run the Step 1 `modprobe`/`ip link` commands; update WSL if it lacks `vcan`. |
| Linux build ignores diagnostics/testpoint options | Add Step 4e to `rpi/CMakeLists.txt`; `run-sa.sh` configures that file, not the Windows root CMake file. |
| `mysql_driver.h` is missing | Install `libmysqlcppconn-dev` and ensure the Linux database target includes/links Connector/C++. |
| GUI row is inserted but no DB testpoint appears | Confirm simulator DB environment overrides are applied before `cDBMessageService` is constructed and that diagnostics/testpoints were compiled into the Linux target. |
| GUI journey is skipped in a full run | The branch has no compatible DB integration. This is expected; the physical journey continues. Use plant-only for the minimum acceptance test. |
