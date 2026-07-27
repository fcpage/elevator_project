#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
state_root="${XDG_STATE_HOME:-${HOME}/.local/state}/project6-elevator-simulator"
venv_root="${state_root}/venv"
venv_python="${venv_root}/bin/python"
schema_profile="agreed-v1"
recreate_schema=false

while (($# > 0)); do
    case "$1" in
        --schema-profile)
            shift
            if (($# == 0)); then
                echo "--schema-profile requires a name." >&2
                exit 2
            fi
            schema_profile="$1"
            ;;
        --recreate-schema)
            recreate_schema=true
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Usage: ./simulator/setup.sh [--schema-profile NAME] [--recreate-schema]" >&2
            exit 2
            ;;
    esac
    shift
done

find_supported_python() {
    local candidate
    for candidate in python3.12 python3.11 python3; do
        if ! command -v "${candidate}" >/dev/null 2>&1; then
            continue
        fi
        if "${candidate}" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 11) else 1)'; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done
    return 1
}

if [[ ! -x "${venv_python}" ]]; then
    if ! supported_python="$(find_supported_python)"; then
        echo "Python 3.11+ is required." >&2
        echo "On Ubuntu 22.04, install Python 3.11 and its venv package:" >&2
        echo "  sudo add-apt-repository ppa:deadsnakes/ppa" >&2
        echo "  sudo apt update && sudo apt install python3.11 python3.11-venv" >&2
        exit 1
    fi
    "${supported_python}" -m venv "${venv_root}"
fi

if ! "${venv_python}" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 11) else 1)'; then
    echo "The existing simulator virtual environment uses Python older than 3.11." >&2
    echo "Install Python 3.11+, then remove '${venv_root}' and rerun setup." >&2
    exit 1
fi
"${venv_python}" -m pip install --disable-pip-version-check "mysql-connector-python==9.0.0"
setup_arguments=(-m sim setup --schema-profile "${schema_profile}")
if [[ "${recreate_schema}" == true ]]; then
    setup_arguments+=(--recreate-schema)
fi
PYTHONPATH="${script_dir}" "${venv_python}" "${setup_arguments[@]}"

echo
echo "Setup complete."
echo "Start the simulator with: ./simulator/run.sh"
