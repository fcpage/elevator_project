#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
state_root="${XDG_STATE_HOME:-${HOME}/.local/state}/project6-elevator-simulator"
venv_python="${state_root}/venv/bin/python"

if [[ ! -x "${venv_python}" ]]; then
    echo "Simulator setup has not been run. Run ./simulator/setup.sh first." >&2
    exit 1
fi

arguments=(-m sim run --transport socketcan)
scenario_path=""
plant_only=false
schema_profile="agreed-v1"
while (($# > 0)); do
    case "$1" in
        --verbose|-v)
            arguments+=(--verbose)
            ;;
        --scenario)
            shift
            if (($# == 0)); then
                echo "--scenario requires a path." >&2
                exit 2
            fi
            scenario_path="$1"
            ;;
        --schema-profile)
            shift
            if (($# == 0)); then
                echo "--schema-profile requires a name." >&2
                exit 2
            fi
            schema_profile="$1"
            ;;
        --plant-only)
            plant_only=true
            ;;
        -*)
            echo "Unknown option: $1" >&2
            echo "Usage: ./simulator/run.sh [--verbose] [--plant-only] [--schema-profile NAME] [--scenario PATH|PATH]" >&2
            exit 2
            ;;
        *)
            if [[ -n "${scenario_path}" ]]; then
                echo "Only one scenario may be selected." >&2
                exit 2
            fi
            scenario_path="$1"
            ;;
    esac
    shift
done

arguments+=(--schema-profile "${schema_profile}")

if [[ -n "${scenario_path}" ]]; then
    if [[ "${scenario_path}" != /* ]]; then
        scenario_path="${script_dir}/${scenario_path}"
    fi
    arguments+=(--scenario "${scenario_path}")
fi

if [[ "${plant_only}" == true ]]; then
    arguments+=(--plant-only)
fi

PYTHONPATH="${script_dir}" "${venv_python}" "${arguments[@]}"
