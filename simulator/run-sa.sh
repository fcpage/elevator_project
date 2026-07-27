#!/usr/bin/env bash

set -Eeuo pipefail

verbose_build=false
while (($# > 0)); do
    case "$1" in
        --verbose-build|-v)
            verbose_build=true
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Usage: ./simulator/run-sa.sh [--verbose-build]" >&2
            exit 2
            ;;
    esac
    shift
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
project_root="$(cd -- "${script_dir}/.." >/dev/null && pwd)"
sa_root="${project_root}/SupervisoryController"
state_root="${XDG_STATE_HOME:-${HOME}/.local/state}/project6-elevator-simulator"
session_path="${state_root}/runtime/active-session.json"
venv_python="${state_root}/venv/bin/python"

if [[ ! -f "${session_path}" ]]; then
    echo "No active simulator session. Start ./simulator/run.sh in the first terminal." >&2
    exit 1
fi

eval "$("${venv_python}" - "${session_path}" <<'PY'
import json
import shlex
import sys

session = json.load(open(sys.argv[1], encoding="utf-8"))
values = {
    "SESSION_TRANSPORT": session["transport"],
    "SESSION_MODE": session.get("mode", "full"),
    "CAN_INTERFACE": session["can_interface"],
    "ELEVATOR_SIM_DIAGNOSTICS_HOST": session["diagnostics_host"],
    "ELEVATOR_SIM_DIAGNOSTICS_PORT": str(session["diagnostics_port"]),
    "ELEVATOR_DB_URL": session["database_url"],
    "ELEVATOR_DB_USER": session["database_user"],
    "ELEVATOR_DB_PASSWORD": session["database_password"],
    "ELEVATOR_DB_SCHEMA": session["database_schema"],
    "SA_LOG_PATH": session["sa_log_path"],
    "SIMULATOR_PID": str(session["simulator_pid"]),
}
for key, value in values.items():
    print(f"{key}={shlex.quote(value)}")
PY
)"

if ! kill -0 "${SIMULATOR_PID}" 2>/dev/null; then
    echo "The active simulator session is stale. Start ./simulator/run.sh again." >&2
    exit 1
fi

if [[ "${SESSION_TRANSPORT}" != "socketcan" ]]; then
    echo "The active session is not using the Linux SocketCAN transport." >&2
    exit 1
fi

if [[ "${SESSION_MODE}" != "plant_only" ]]; then
    export ELEVATOR_SIM_DIAGNOSTICS_HOST
    export ELEVATOR_SIM_DIAGNOSTICS_PORT
    export ELEVATOR_DB_URL
    export ELEVATOR_DB_USER
    export ELEVATOR_DB_PASSWORD
    export ELEVATOR_DB_SCHEMA
fi

build_root="${state_root}/build/linux"
mkdir -p "$(dirname -- "${SA_LOG_PATH}")"
printf '%s\n' "=== SA build ===" > "${SA_LOG_PATH}"

run_build_phase() {
    local label="$1"
    shift
    local phase_log="${SA_LOG_PATH}.phase"

    printf '%s... ' "${label}"
    set +e
    "$@" > "${phase_log}" 2>&1
    local status=$?
    set -e

    printf '=== %s ===\n' "${label}" >> "${SA_LOG_PATH}"
    cat "${phase_log}" >> "${SA_LOG_PATH}"
    if [[ "${verbose_build}" == true ]]; then
        printf '\n'
        cat "${phase_log}"
    fi

    if ((status != 0)); then
        printf 'FAILED\n' >&2
        grep -Ei '\b(error|fatal|failed)\b' "${phase_log}" | tail -n 20 >&2 ||
            tail -n 20 "${phase_log}" >&2
        printf '%s failed. Full details: %s\n' "${label}" "${SA_LOG_PATH}" >&2
        return "${status}"
    fi
    printf 'OK\n'
}

cmake_arguments=(
    -S "${sa_root}/rpi" -B "${build_root}"
    -DSUPERVISORY_ENABLE_AUTO_ARRIVAL=OFF
    -DSUPERVISORY_USE_VIRTUAL_CAN=ON
    -DSUPERVISORY_CAN_INTERFACE_PRECONFIGURED=ON
)
if [[ "${SESSION_MODE}" != "plant_only" ]]; then
    cmake_arguments+=(
        -DSUPERVISORY_ENABLE_SIM_DIAGNOSTICS=ON
        -DSUPERVISORY_ENABLE_SIM_TESTPOINTS=ON
    )
fi
run_build_phase "CMake configure" cmake "${cmake_arguments[@]}"
run_build_phase "Build supervisory_controller" \
    cmake --build "${build_root}" --config Release --target supervisory_controller

printf 'Build log: %s\n' "${SA_LOG_PATH}"
if [[ "${SESSION_MODE}" == "plant_only" ]]; then
    printf '%s\n' "Plant-only session: database and diagnostics are intentionally disabled."
fi
bash "${sa_root}/scripts/initialize_can.sh" --virtual "${CAN_INTERFACE}"

printf '%s\n' "=== SA runtime ===" | tee -a "${SA_LOG_PATH}"
set +e
"${build_root}/supervisory_controller" "${CAN_INTERFACE}" 2>&1 | tee -a "${SA_LOG_PATH}"
sa_exit=${PIPESTATUS[0]}
set -e
if ((sa_exit != 0)); then
    printf 'SA exited with code %d. Full details: %s\n' "${sa_exit}" "${SA_LOG_PATH}" >&2
fi
exit "${sa_exit}"
