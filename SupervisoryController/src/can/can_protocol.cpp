/******************************************************************
* can_protocol.cpp - Shared CAN Protocol Helpers
* Author: Project 6 Team
* Last Modified: 2026-06-01
* @brief Implements the shared one-byte CAN frame layout.
******************************************************************/

#include "project6/supervisory/can/can_protocol.hpp"

namespace project6::supervisory
{

namespace
{

bool isValidFloor(const std::uint8_t floor, const CanProtocolConfig& config)
{
    return floor >= config.minFloor && floor <= config.maxFloor;
} // namespace

std::optional<std::uint8_t> floorFromPayload(
    const std::uint8_t payload,
    const CanProtocolConfig& config)
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
    const CanProtocolConfig& config)
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

bool isFloorControllerId(const std::uint16_t id, const CanProtocolConfig& config)
{
    return floorFromFloorControllerId(id, config).has_value();
}

}

std::optional<DecodedCanMessage> decodeCanFrame(const CanFrame& frame, const CanProtocolConfig& config)
{
    if (frame.dataLength != config.sharedProtocolDlc)
    {
        return std::nullopt;
    }

    const std::uint8_t payload = frame.data[0];
    DecodedCanMessage message{};
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

std::optional<SupervisoryEvent> toSupervisoryEvent(const DecodedCanMessage& message)
{
    SupervisoryEvent event{};

    switch (message.type)
    {
        case CanMessageType::ElevatorStatus:
        {
            if (!message.floor.has_value())
            {
                return std::nullopt;
            }

            event.type = EventType::CanElevatorStatus;
            event.reportedFloor = message.floor;
            event.reportedDirection = TravelDirection::None;
            return event;
        }

        case CanMessageType::CarFloorRequest:
        {
            if (!message.floor.has_value())
            {
                return std::nullopt;
            }

            event.type = EventType::CanCarRequest;
            event.requestedFloor = message.floor;
            return event;
        }

        case CanMessageType::FloorModuleRequest:
        {
            if (!message.floor.has_value())
            {
                return std::nullopt;
            }

            event.type = EventType::CanFloorRequest;
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

std::optional<CanFrame> makeSupervisorCommandFrame(
    std::uint8_t targetFloor,
    bool enable,
    const CanProtocolConfig& config)
{
    if (!isValidFloor(targetFloor, config) || config.floorShift > 7)
    {
        return std::nullopt;
    }

    CanFrame frame{};
    frame.id = config.supervisoryControllerCanId;
    frame.dataLength = config.sharedProtocolDlc;

    const std::uint8_t shiftedFloor = static_cast<std::uint8_t>(targetFloor << config.floorShift);
    const std::uint8_t encodedFloor = static_cast<std::uint8_t>(shiftedFloor & config.floorMask);
    const std::uint8_t encodedEnable = enable ? config.statusOrEnableMask : 0;

    frame.data[0] = static_cast<std::uint8_t>(encodedEnable | encodedFloor);
    return frame;
}

} // namespace project6::supervisory
