/******************************************************************
* can_protocol.cpp - Shared CAN Protocol Helpers
* Author: Project 6 Team
* Last Modified: 2026-06-01
* @file can_protocol.cpp
* @brief Implements the shared one-byte CAN frame layout.
******************************************************************/

#include "supervisory/can/can_protocol.hpp"

namespace project6::supervisory
{

namespace
{

bool isValidFloor(const std::uint8_t floor, const sCanProtocolConfig& config)
{
    return floor >= config.minFloor && floor <= config.maxFloor;
}

std::optional<std::uint8_t> floorFromPayload(
    const std::uint8_t payload,
    const sCanProtocolConfig& config)
{
    if (config.floorShift > 7)
    {
        return std::nullopt;
    }

    const auto floor = static_cast<std::uint8_t>((payload & config.floorMask) >> config.floorShift);
    if (!isValidFloor(floor, config))
    {
        return std::nullopt;
    }

    return floor;
}

std::optional<std::uint8_t> floorFromFloorControllerId(
    const std::uint16_t id,
    const sCanProtocolConfig& config)
{
    if (id == config.floorOneControllerCanId)
    {
        return static_cast<std::uint8_t>(1);
    }

    if (id == config.floorTwoControllerCanId)
    {
        return static_cast<std::uint8_t>(2);
    }

    if (id == config.floorThreeControllerCanId)
    {
        return static_cast<std::uint8_t>(3);
    }

    return std::nullopt;
}

bool isFloorControllerId(const std::uint16_t id, const sCanProtocolConfig& config)
{
    return floorFromFloorControllerId(id, config).has_value();
}

} // namespace

std::optional<sDecodedCanMessage> decodeCanFrame(const sCanFrame& frame, const sCanProtocolConfig& config)
{
    if (frame.dataLength != config.sharedProtocolDlc)
    {
        return std::nullopt;
    }

    const std::uint8_t payload = frame.data[0];
    sDecodedCanMessage message{};
    message.sourceId = frame.id;

    if (frame.id == config.elevatorControllerCanId)
    {
        const std::optional<std::uint8_t> floor = floorFromPayload(payload, config);
        if (!floor.has_value())
        {
            return std::nullopt;
        }

        message.type = CanMessageType::ElevatorStatus;
        message.floor = floor;
        message.statusBit = (payload & config.statusOrEnableMask) != 0;
        return message;
    }

    if (frame.id == config.carControllerCanId)
    {
        const std::optional<std::uint8_t> floor = floorFromPayload(payload, config);
        if (!floor.has_value())
        {
            return std::nullopt;
        }

        message.type = CanMessageType::CarFloorRequest;
        message.floor = floor;
        message.statusBit = (payload & config.statusOrEnableMask) != 0;
        return message;
    }

    if (isFloorControllerId(frame.id, config))
    {
        if ((payload & config.floorModuleRequestMask) == 0)
        {
            return std::nullopt;
        }

        message.type = CanMessageType::FloorModuleRequest;
        message.floor = floorFromFloorControllerId(frame.id, config);
        return message;
    }

    return std::nullopt;
}

std::optional<sSupervisoryEvent> toSupervisoryEvent(const sDecodedCanMessage& message)
{
    sSupervisoryEvent event{};

    switch (message.type)
    {
        case CanMessageType::ElevatorStatus:
        {
            if (!message.floor.has_value())
            {
                return std::nullopt;
            }

            event.type = ecEventType::CanElevatorStatus;
            event.reportedFloor = message.floor;
            event.reportedDirection = ecTravelDirection::None;
            return event;
        }

        case CanMessageType::CarFloorRequest:
        {
            if (!message.floor.has_value())
            {
                return std::nullopt;
            }

            event.type = ecEventType::CanCarRequest;
            event.requestedFloor = message.floor;
            return event;
        }

        case CanMessageType::FloorModuleRequest:
        {
            if (!message.floor.has_value())
            {
                return std::nullopt;
            }

            event.type = ecEventType::CanFloorRequest;
            event.requestedFloor = message.floor;
            return event;
        }

        case CanMessageType::SupervisorCommand:
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<sCanFrame> makeSupervisorCommandFrame(
    std::uint8_t targetFloor,
    bool enable,
    const sCanProtocolConfig& config)
{
    if (!isValidFloor(targetFloor, config) || config.floorShift > 7)
    {
        return std::nullopt;
    }

    sCanFrame frame{};
    frame.id = config.supervisoryControllerCanId;
    frame.dataLength = config.sharedProtocolDlc;

    const std::uint8_t shiftedFloor = static_cast<std::uint8_t>(targetFloor << config.floorShift);
    const std::uint8_t encodedFloor = static_cast<std::uint8_t>(shiftedFloor & config.floorMask);
    const std::uint8_t encodedEnable = enable ? config.statusOrEnableMask : 0;

    frame.data[0] = static_cast<std::uint8_t>(encodedEnable | encodedFloor);
    return frame;
}

} // namespace project6::supervisory
