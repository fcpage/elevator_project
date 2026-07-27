from __future__ import annotations

from sim.can import CanFrame


SC_CAN_ID = 0x100
EC_CAN_ID = 0x101
CC_CAN_ID = 0x200
FLOOR_CAN_IDS = {
    1: 0x201,
    2: 0x202,
    3: 0x203,
}

CAN_BITRATE_BITS_PER_SECOND = 250_000
MIN_FLOOR = 1
MAX_FLOOR = 3
SHARED_PROTOCOL_DLC = 1
FLOOR_MASK = 0x03
FLOOR_SHIFT = 0
STATUS_OR_ENABLE_MASK = 0x04
FLOOR_MODULE_REQUEST_MASK = 0x01
INTERNAL_PROTOCOL_FLAG_MASK = 0x80
HB_OK = 0x84
HB_SC_REQUEST = 0x85
HB_NODE_REQUEST = 0x86
HB_ERROR = 0x87
SC_DOOR_OPEN = 0x88
SC_DOOR_CLOSE = 0x89

SC_ENABLE = STATUS_OR_ENABLE_MASK
EC_STATUS_IDLE = 0x00
EC_STATUS_MOVING = STATUS_OR_ENABLE_MASK


def is_valid_floor(floor: int) -> bool:
    return MIN_FLOOR <= floor <= MAX_FLOOR


def floor_from_payload(payload: int) -> int:
    return (payload & FLOOR_MASK) >> FLOOR_SHIFT


def payload_has_status_or_enable(payload: int) -> bool:
    return (payload & STATUS_OR_ENABLE_MASK) != 0


def is_internal_payload(payload: int) -> bool:
    return (payload & INTERNAL_PROTOCOL_FLAG_MASK) != 0


def is_heartbeat_payload(payload: int) -> bool:
    return payload in (HB_OK, HB_SC_REQUEST, HB_NODE_REQUEST, HB_ERROR)


def describe_frame(frame: CanFrame) -> str:
    payload = frame.data[0] if frame.data else None
    if payload is None:
        return "empty"
    if frame.can_id == SC_CAN_ID and payload == HB_SC_REQUEST:
        return "supervisor_heartbeat_request"
    if payload == HB_OK:
        return "heartbeat_ok"
    if frame.can_id == SC_CAN_ID and payload == SC_DOOR_OPEN:
        return "door_open"
    if frame.can_id == SC_CAN_ID and payload == SC_DOOR_CLOSE:
        return "door_close"
    if frame.can_id == SC_CAN_ID and not is_internal_payload(payload):
        return f"dispatch_floor_{floor_from_payload(payload)}"
    if frame.can_id == EC_CAN_ID:
        return f"elevator_status_floor_{floor_from_payload(payload)}"
    if frame.can_id == CC_CAN_ID:
        return f"car_request_floor_{floor_from_payload(payload)}"
    if frame.can_id in FLOOR_CAN_IDS.values():
        return "floor_call"
    return "unknown"


def encode_supervisor_command(floor: int, *, enable: bool) -> CanFrame:
    if not is_valid_floor(floor):
        raise ValueError("supervisor command floor must be 1, 2, or 3")

    encoded_floor = (floor << FLOOR_SHIFT) & FLOOR_MASK
    encoded_enable = STATUS_OR_ENABLE_MASK if enable else 0
    return CanFrame(can_id=SC_CAN_ID, data=[encoded_enable | encoded_floor], source="supervisor")
