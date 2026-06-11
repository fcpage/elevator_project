#!/usr/bin/env bash

set -Eeuo pipefail

usage()
{
    cat <<'USAGE'
Usage:
  initialize_can.sh --virtual [interface]
  initialize_can.sh [interface] [bitrate] [restart_ms]

Examples:
  ./scripts/initialize_can.sh --virtual vcan0
  ./scripts/initialize_can.sh can0 125000
  ./scripts/initialize_can.sh can1 500000 100
USAGE
}

require_command()
{
    local command_name="$1"

    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "initialize_can.sh: missing required command: ${command_name}" >&2
        return 1
    fi
}

run_privileged()
{
    if [[ "${EUID}" -eq 0 ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

validate_interface_name()
{
    local interface_name="$1"

    if [[ ! "${interface_name}" =~ ^[A-Za-z0-9_.:-]+$ ]]; then
        echo "initialize_can.sh: invalid interface name: ${interface_name}" >&2
        return 1
    fi
}

validate_uint()
{
    local value="$1"
    local label="$2"

    if [[ ! "${value}" =~ ^[0-9]+$ ]] || [[ "${value}" -eq 0 ]]; then
        echo "initialize_can.sh: ${label} must be a positive integer: ${value}" >&2
        return 1
    fi
}

validate_nonnegative_uint()
{
    local value="$1"
    local label="$2"

    if [[ ! "${value}" =~ ^[0-9]+$ ]]; then
        echo "initialize_can.sh: ${label} must be a non-negative integer: ${value}" >&2
        return 1
    fi
}

require_privilege_tool()
{
    if [[ "${EUID}" -ne 0 ]]; then
        require_command sudo
    fi
}

interface_exists()
{
    local interface_name="$1"

    ip link show dev "${interface_name}" >/dev/null 2>&1
}

setup_virtual_can()
{
    local interface_name="$1"

    validate_interface_name "${interface_name}"
    require_command ip
    require_command modprobe
    require_privilege_tool

    run_privileged modprobe vcan

    if ! interface_exists "${interface_name}"; then
        run_privileged ip link add dev "${interface_name}" type vcan
    fi

    run_privileged ip link set dev "${interface_name}" up

    echo "initialize_can.sh: virtual CAN ready on ${interface_name}"
    ip -details -statistics link show "${interface_name}"
}

setup_physical_can()
{
    local interface_name="$1"
    local bitrate="$2"
    local restart_ms="$3"

    validate_interface_name "${interface_name}"
    validate_uint "${bitrate}" "bitrate"
    validate_nonnegative_uint "${restart_ms}" "restart_ms"
    require_command ip
    require_command modprobe
    require_privilege_tool

    run_privileged modprobe can
    run_privileged modprobe can_raw
    run_privileged modprobe peak_usb >/dev/null 2>&1 || true

    if ! interface_exists "${interface_name}"; then
        echo "initialize_can.sh: ${interface_name} does not exist." >&2
        echo "initialize_can.sh: connect the CAN adapter, then check: lsusb; ip -br link; dmesg | grep -i can" >&2
        ip -br link >&2
        return 1
    fi

    run_privileged ip link set dev "${interface_name}" down || true
    run_privileged ip link set dev "${interface_name}" type can bitrate "${bitrate}" restart-ms "${restart_ms}"
    run_privileged ip link set dev "${interface_name}" up

    echo "initialize_can.sh: physical CAN ready on ${interface_name} at ${bitrate} bit/s"
    ip -details -statistics link show "${interface_name}"
}

main()
{
    if [[ "${1:-}" == "--help" ]] || [[ "${1:-}" == "-h" ]]; then
        usage
        return 0
    fi

    if [[ "${1:-}" == "--virtual" ]]; then
        setup_virtual_can "${2:-vcan0}"
        return 0
    fi

    setup_physical_can "${1:-can0}" "${2:-125000}" "${3:-100}"
}

main "$@"
