#!/usr/bin/env bash

set -Eeuo pipefail

usage()
{
    cat <<'USAGE'
Usage:
  run_rpi.sh [--test|--hardware|--production] [interface] [bitrate]
  run_rpi.sh --sabbath [--test|--hardware|--production] [interface] [bitrate]
  run_rpi.sh --maintenance [--test|--hardware|--production] [interface] [bitrate]

Examples:
  ./scripts/run_rpi.sh --test
  ./scripts/run_rpi.sh --hardware can0 125000
  ./scripts/run_rpi.sh --production can0 125000
  ./scripts/run_rpi.sh --sabbath
  ./scripts/run_rpi.sh --maintenance
USAGE
}

require_command()
{
    local command_name="$1"
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "run_rpi.sh: missing required command: ${command_name}" >&2
        return 1
    fi
}

script_directory()
{
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null
    pwd
}

parse_arguments()
{
    transport_mode="test"
    feature_mode=""
    positional=()
    while (($# > 0)); do
        case "$1" in
            --test|test) transport_mode="test" ;;
            --hardware|hardware) transport_mode="hardware" ;;
            --production|production) transport_mode="production" ;;
            --sabbath)
                [[ -z "${feature_mode}" ]] || { echo "Choose either --sabbath or --maintenance, not both." >&2; return 1; }
                feature_mode="sabbath"
                ;;
            --maintenance)
                [[ -z "${feature_mode}" ]] || { echo "Choose either --sabbath or --maintenance, not both." >&2; return 1; }
                feature_mode="maintenance"
                ;;
            --help|-h) usage; exit 0 ;;
            --*) echo "run_rpi.sh: unknown option: $1" >&2; return 1 ;;
            *) positional+=("$1") ;;
        esac
        shift
    done
    if [[ "${transport_mode}" == "test" ]]; then
        interface_name="${positional[0]:-vcan0}"
        bitrate=""
        [[ ${#positional[@]} -le 1 ]] || { echo "run_rpi.sh: test mode accepts only an interface." >&2; return 1; }
    else
        interface_name="${positional[0]:-can0}"
        bitrate="${positional[1]:-125000}"
        [[ ${#positional[@]} -le 2 ]] || { echo "run_rpi.sh: expected [interface] [bitrate]." >&2; return 1; }
    fi
}

main()
{
    require_command tee
    require_command date

    local script_dir
    script_dir="$(script_directory)"
    local supervisor_root
    supervisor_root="$(cd -- "${script_dir}/.." >/dev/null && pwd)"
    parse_arguments "$@"

    local build_mode="${transport_mode}"
    if [[ -n "${feature_mode}" ]]; then
        build_mode+="-${feature_mode}"
    fi
    local build_directory="${supervisor_root}/build-rpi/${build_mode}"
    local executable_path="${build_directory}/supervisory_controller"

    # Running a demo command is intentionally self-healing: if its matching
    # build does not exist, build it using the exact same parsed arguments.
    if [[ ! -f "${executable_path}" ]]; then
        bash "${script_dir}/build_rpi.sh" "$@"
    fi
    if [[ ! -f "${executable_path}" ]]; then
        echo "run_rpi.sh: build did not produce ${executable_path}" >&2
        return 1
    fi
    [[ -x "${executable_path}" ]] || chmod +x "${executable_path}"

    local timestamp
    timestamp="$(date +%Y%m%d-%H%M%S)"
    local log_path="${supervisor_root}/supervisory-${timestamp}.log"
    if [[ "${transport_mode}" == "test" ]]; then
        bash "${script_dir}/initialize_can.sh" --virtual "${interface_name}"
    else
        bash "${script_dir}/initialize_can.sh" "${interface_name}" "${bitrate}"
    fi

    echo "run_rpi.sh: logging to ${log_path}"
    if [[ -n "${feature_mode}" ]]; then
        local control_file="${build_directory}/demo_control.txt"
        if [[ ! -f "${control_file}" ]]; then
            cp "${supervisor_root}/demo_control.${feature_mode}.txt" "${control_file}"
        fi
        SUPERVISORY_DEMO_CONTROL_FILE="${control_file}" \
        SUPERVISORY_AUDIO_DIR="${build_directory}/audio" \
            "${executable_path}" "${interface_name}" 2>&1 | tee "${log_path}"
    else
        SUPERVISORY_AUDIO_DIR="${build_directory}/audio" \
            "${executable_path}" "${interface_name}" 2>&1 | tee "${log_path}"
    fi
}

main "$@"
