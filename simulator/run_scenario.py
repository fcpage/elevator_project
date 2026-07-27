from __future__ import annotations

import argparse
import shlex

from sim.runner import run_builtin_scenario, run_process_scenario
from sim.supervisor import InProcessSupervisor
from sim.supervisor_process import JsonLineSupervisorProcess


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a Project 6 simulator scenario.")
    parser.add_argument(
        "scenario",
        nargs="?",
        default="basic_floor_request",
        help="Built-in scenario name. Default: basic_floor_request",
    )
    parser.add_argument(
        "--supervisor-command",
        help="Command that starts a JSON-lines C++ supervisor process.",
    )
    args = parser.parse_args()

    if args.supervisor_command:
        process = JsonLineSupervisorProcess(shlex.split(args.supervisor_command, posix=False))
        result = run_process_scenario(args.scenario, process)
    else:
        result = run_builtin_scenario(args.scenario, InProcessSupervisor())
    for event in result.events:
        print(event)
    print(f"final_floor:{result.final_floor}")
    print("PASS" if result.passed else "FAIL")
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
