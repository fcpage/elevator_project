# Simulator Integration Kit

This directory is the copy/paste source for adopting the elevator simulator on
a branch that has only legacy simulator adapters. It is deliberately arranged
as a small library of functions.

For the exact first-time copy/paste order, use
[LEGACY_BRANCH_QUICKSTART.md](LEGACY_BRANCH_QUICKSTART.md). Do not start with
the CMake fragment; the quickstart puts it after the required file copies and
`main.cpp` seam.

## How to use

Copy the files under `cpp/` into the same relative paths beneath the branch's
`SupervisoryController` directory. Then apply the snippets under `snippets/` to
the branch's *existing* `main.cpp` and `CMakeLists.txt`.

## Contents

| Path | Copy destination / purpose |
| --- | --- |
| `cpp/include/supervisory/can/runtime_can_service.hpp` | `include/supervisory/can/`; selects the production or simulator CAN service at build time. |
| `cpp/include/supervisory/sim/simulator_can_service.hpp` | `include/supervisory/sim/`; required localhost CAN bridge API. |
| `cpp/src/sim/simulator_can_service.cpp` | `src/sim/`; required localhost CAN bridge implementation. |
| `cpp/include/supervisory/sim/simulator_diagnostics.hpp` and `cpp/src/sim/simulator_diagnostics.cpp` | Optional diagnostics add-on for a branch with the baseline database exchange. |
| `cpp/include/project6_sim/testpoints.hpp` | `include/project6_sim/`; optional application-facing testpoint macro. |
| `cmake/simulator_integration.cmake` | Copy its commented options/target blocks into the branch CMake configuration; it also includes the Windows `ws2_32` requirement. |
| `snippets/*.txt` | Commented placement examples; they are reference text and intentionally do not compile alone. |

## Minimal legacy-branch adoption

1. Copy `runtime_can_service.hpp`, `simulator_can_service.hpp`, and
   `simulator_can_service.cpp` into the branch.
2. Replace direct `cCanCommsService` construction in the existing main with
   `cRuntimeCanService`, using `snippets/main_transport_selection.cpp.txt`.
3. Add the supplied source files and options to the existing CMake targets using
   `cmake/simulator_integration.cmake`.
4. Copy the diagnostics/testpoint add-on only when the branch has the baseline
   database exchange it requires. A branch with no testpoints is fully
   supported.
5. Use `run` then `run-sa`. The launcher builds the branch's normal executable.

For the complete host setup and two-terminal workflow, see
[`../MANUAL.md`](../MANUAL.md).
