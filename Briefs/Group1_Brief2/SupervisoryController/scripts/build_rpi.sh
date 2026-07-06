#!/usr/bin/env bash

set -Eeuo pipefail

usage()
{
    cat <<'USAGE'
Usage:
  build_rpi.sh --test [interface]
  build_rpi.sh --hardware [interface] [bitrate]
  build_rpi.sh --production [interface] [bitrate]

Modes:
  --test        Build for vcan dry runs. Uses virtual CAN and auto-arrival.
  --hardware    Build for physical CAN demo testing. Uses auto-arrival.
  --production  Build for physical CAN with real elevator-status arrivals only.

Examples:
  ./scripts/build_rpi.sh --test
  ./scripts/build_rpi.sh --hardware can0 250000
  ./scripts/build_rpi.sh --production can0 250000
USAGE
}

require_command()
{
    local command_name="$1"

    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "build_rpi.sh: missing required command: ${command_name}" >&2
        return 1
    fi
}

script_directory()
{
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null
    pwd
}

select_cmake_source()
{
    local supervisor_root="$1"

    if [[ -f "${supervisor_root}/rpi/CMakeLists.txt" ]]; then
        echo "${supervisor_root}/rpi"
        return 0
    fi

    if [[ -f "${supervisor_root}/CMakeLists.txt" ]]; then
        echo "${supervisor_root}"
        return 0
    fi

    echo "build_rpi.sh: could not find an RPi CMakeLists.txt" >&2
    return 1
}

run_cmake_build()
{
    local cmake_source="$1"
    local build_directory="$2"
    local enable_auto_arrival="$3"
    local use_virtual_can="$4"

    cmake \
        -S "${cmake_source}" \
        -B "${build_directory}" \
        -DSUPERVISORY_ENABLE_AUTO_ARRIVAL="${enable_auto_arrival}" \
        -DSUPERVISORY_USE_VIRTUAL_CAN="${use_virtual_can}"

    cmake --build "${build_directory}"
}

main()
{
    local mode="${1:-}"
    if [[ "${mode}" == "--help" ]] || [[ "${mode}" == "-h" ]]; then
        usage
        return 0
    fi

    require_command cmake

    local script_dir
    script_dir="$(script_directory)"
    local supervisor_root
    supervisor_root="$(cd -- "${script_dir}/.." >/dev/null && pwd)"
    local cmake_source
    cmake_source="$(select_cmake_source "${supervisor_root}")"
    local build_directory="${supervisor_root}/build-rpi"

    case "${mode}" in
        --test|test)
            local interface_name="${2:-vcan0}"
            bash "${script_dir}/initialize_can.sh" --virtual "${interface_name}"
            run_cmake_build "${cmake_source}" "${build_directory}" ON ON
            echo "build_rpi.sh: run ./build-rpi/supervisory_controller ${interface_name}"
            ;;

        --hardware|hardware)
            local interface_name="${2:-can0}"
            local bitrate="${3:-250000}"
            bash "${script_dir}/initialize_can.sh" "${interface_name}" "${bitrate}"
            run_cmake_build "${cmake_source}" "${build_directory}" ON OFF
            echo "build_rpi.sh: run sudo ./build-rpi/supervisory_controller ${interface_name}"
            ;;

        --production|production)
            local interface_name="${2:-can0}"
            local bitrate="${3:-250000}"
            bash "${script_dir}/initialize_can.sh" "${interface_name}" "${bitrate}"
            run_cmake_build "${cmake_source}" "${build_directory}" OFF OFF
            echo "build_rpi.sh: run sudo ./build-rpi/supervisory_controller ${interface_name}"
            ;;

        *)
            usage >&2
            return 1
            ;;
    esac
}

main "$@"
