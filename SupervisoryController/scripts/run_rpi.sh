#!/usr/bin/env bash

set -Eeuo pipefail

usage()
{
    cat <<'USAGE'
Usage:
  run_rpi.sh --test [interface]
  run_rpi.sh --hardware [interface] [bitrate]
  run_rpi.sh --production [interface] [bitrate]

Modes:
  --test        Initialize virtual CAN and run the virtual-CAN build.
  --hardware    Initialize physical CAN and run the hardware demo build.
  --production  Initialize physical CAN and run the production build.

Examples:
  ./scripts/run_rpi.sh --test
  ./scripts/run_rpi.sh --hardware can0 125000
  ./scripts/run_rpi.sh --production can0 125000
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

require_executable()
{
    local executable_path="$1"

    if [[ ! -f "${executable_path}" ]]; then
        echo "run_rpi.sh: application has not been built: ${executable_path}" >&2
        echo "run_rpi.sh: run scripts/build_rpi.sh with the same mode first." >&2
        return 1
    fi

    if [[ ! -x "${executable_path}" ]]; then
        chmod +x "${executable_path}"
    fi
}

run_application()
{
    local executable_path="$1"
    local interface_name="$2"
    local log_path="$3"

    echo "run_rpi.sh: logging to ${log_path}"
    "${executable_path}" "${interface_name}" 2>&1 | tee "${log_path}"
}

main()
{
    local mode="${1:-}"
    if [[ "${mode}" == "--help" ]] || [[ "${mode}" == "-h" ]]; then
        usage
        return 0
    fi

    case "${mode}" in
        --test|test|--hardware|hardware|--production|production)
            ;;

        *)
            usage >&2
            return 1
            ;;
    esac

    require_command tee
    require_command date

    local script_dir
    script_dir="$(script_directory)"
    local supervisor_root
    supervisor_root="$(cd -- "${script_dir}/.." >/dev/null && pwd)"
    local build_mode
    case "${mode}" in
        --test|test)
            build_mode="test"
            ;;
        --hardware|hardware)
            build_mode="hardware"
            ;;
        --production|production)
            build_mode="production"
            ;;
    esac
    local executable_path="${supervisor_root}/build-rpi/${build_mode}/supervisory_controller"

    require_executable "${executable_path}"

    local timestamp
    timestamp="$(date +%Y%m%d-%H%M%S)"
    local log_path="${supervisor_root}/supervisory-${timestamp}.log"

    case "${mode}" in
        --test|test)
            local interface_name="${2:-vcan0}"
            bash "${script_dir}/initialize_can.sh" --virtual "${interface_name}"
            run_application "${executable_path}" "${interface_name}" "${log_path}"
            ;;

        --hardware|hardware|--production|production)
            local interface_name="${2:-can0}"
            local bitrate="${3:-125000}"
            bash "${script_dir}/initialize_can.sh" "${interface_name}" "${bitrate}"
            run_application "${executable_path}" "${interface_name}" "${log_path}"
            ;;

    esac
}

main "$@"
