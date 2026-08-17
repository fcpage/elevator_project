#!/usr/bin/env bash

set -Eeuo pipefail

usage()
{
    cat <<'USAGE'
Usage:
  build_rpi.sh [--test|--hardware|--production] [interface] [bitrate]
  build_rpi.sh --sabbath [--test|--hardware|--production] [interface] [bitrate]
  build_rpi.sh --maintenance [--test|--hardware|--production] [interface] [bitrate]

Defaults:
  no mode flag       test mode on vcan0
  --hardware         hardware mode on can0 at 125000 bit/s
  --production       production mode on can0 at 125000 bit/s
  --sabbath          test mode with Sabbath mode enabled
  --maintenance      test mode with maintenance lock-out enabled

The mode flags may be written in any order. Production and hardware builds
without --sabbath or --maintenance retain their existing behavior.
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
    elif [[ -f "${supervisor_root}/CMakeLists.txt" ]]; then
        echo "${supervisor_root}"
    else
        echo "build_rpi.sh: could not find an RPi CMakeLists.txt" >&2
        return 1
    fi
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
            --*) echo "build_rpi.sh: unknown option: $1" >&2; return 1 ;;
            *) positional+=("$1") ;;
        esac
        shift
    done

    if [[ "${transport_mode}" == "test" ]]; then
        interface_name="${positional[0]:-vcan0}"
        bitrate=""
        [[ ${#positional[@]} -le 1 ]] || { echo "build_rpi.sh: test mode accepts only an interface." >&2; return 1; }
    else
        interface_name="${positional[0]:-can0}"
        bitrate="${positional[1]:-125000}"
        [[ ${#positional[@]} -le 2 ]] || { echo "build_rpi.sh: expected [interface] [bitrate]." >&2; return 1; }
    fi
}

run_cmake_build()
{
    local cmake_source="$1"
    local build_directory="$2"
    local enable_auto_arrival="$3"
    local use_virtual_can="$4"
    local enable_demo_modes="$5"
    local supervisor_root="$6"

    # RPi deployment uses the vendored header and real audio output. The root
    # Windows CMake build remains fake-audio by default for development/tests.
    cmake -S "${cmake_source}" -B "${build_directory}" \
        -DSUPERVISORY_ENABLE_AUTO_ARRIVAL="${enable_auto_arrival}" \
        -DSUPERVISORY_USE_VIRTUAL_CAN="${use_virtual_can}" \
        -DSUPERVISORY_CAN_INTERFACE_PRECONFIGURED=ON \
        -DSUPERVISORY_ENABLE_DEMO_MODES="${enable_demo_modes}" \
        -DSUPERVISORY_ENABLE_MINIAUDIO=ON \
        -DSUPERVISORY_MINIAUDIO_INCLUDE_DIR="${supervisor_root}/include"

    cmake --build "${build_directory}"

    if [[ -n "${feature_mode}" ]]; then
        cp "${supervisor_root}/demo_control.${feature_mode}.txt" "${build_directory}/demo_control.txt"
    fi
}

main()
{
    local script_dir
    script_dir="$(script_directory)"
    local supervisor_root
    supervisor_root="$(cd -- "${script_dir}/.." >/dev/null && pwd)"
    local cmake_source
    cmake_source="$(select_cmake_source "${supervisor_root}")"

    parse_arguments "$@"

    require_command cmake

    if [[ ! -f "${supervisor_root}/include/miniaudio.h" ]]; then
        echo "build_rpi.sh: missing ${supervisor_root}/include/miniaudio.h" >&2
        return 1
    fi

    local build_mode="${transport_mode}"
    if [[ -n "${feature_mode}" ]]; then
        build_mode+="-${feature_mode}"
    fi
    local build_directory="${supervisor_root}/build-rpi/${build_mode}"
    local auto_arrival="OFF"
    local virtual_can="OFF"
    [[ "${transport_mode}" == "test" ]] && auto_arrival="ON" && virtual_can="ON"
    [[ "${transport_mode}" == "hardware" ]] && auto_arrival="ON"

    run_cmake_build "${cmake_source}" "${build_directory}" "${auto_arrival}" "${virtual_can}" \
        "$([[ -n "${feature_mode}" ]] && echo ON || echo OFF)" "${supervisor_root}"

    echo "Build complete: ${build_mode}"
    echo "Run: ./scripts/run_rpi.sh ${transport_mode} ${interface_name} ${bitrate} ${feature_mode:+--${feature_mode}}"
}

main "$@"
