/******************************************************************
* supervisory_drivers.cpp - Supervisory Driver Boundary
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Provides hardware-facing operations used by the state machine.
******************************************************************/

#include "project6/supervisory/drivers/supervisory_drivers.hpp"

#include <iomanip>
#include <iostream>
#include <optional>

#include "project6/supervisory/can/can_adapter.hpp"
#include "project6/supervisory/can/can_protocol.hpp"

namespace project6::supervisory::drivers
{

namespace
{

const char* operationStatusName(const ecOperationStatus status)
{
    switch (status)
    {
        case ecOperationStatus::Ok:
            return "Ok";
        case ecOperationStatus::NotInitialized:
            return "NotInitialized";
        case ecOperationStatus::InvalidArgument:
            return "InvalidArgument";
        case ecOperationStatus::WouldBlock:
            return "WouldBlock";
        case ecOperationStatus::InsufficientPrivileges:
            return "InsufficientPrivileges";
        case ecOperationStatus::HardwareUnavailable:
            return "HardwareUnavailable";
        case ecOperationStatus::NetworkUnavailable:
            return "NetworkUnavailable";
        case ecOperationStatus::NotImplemented:
            return "NotImplemented";
    }

    return "Unknown";
}

} // namespace

ecOperationStatus commandElevatorToFloor(
    cSocketCanAdapter& canAdapter,
    const std::uint8_t targetFloor)
{
    const std::optional<sCanFrame> frame = makeSupervisorCommandFrame(targetFloor, true);
    if (!frame.has_value())
    {
        std::cerr << "CAN_TX_RESULT status=InvalidArgument target_floor="
                  << static_cast<unsigned int>(targetFloor) << '\n';
        return ecOperationStatus::InvalidArgument;
    }

    std::clog << "CAN_TX id=0x" << std::hex << std::uppercase << frame->id
              << std::dec << " dlc=" << static_cast<unsigned int>(frame->dataLength)
              << " data=" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<unsigned int>(frame->data[0])
              << std::dec << std::nouppercase << std::setfill(' ')
              << " target_floor=" << static_cast<unsigned int>(targetFloor) << '\n';

    const ecOperationStatus status = canAdapter.sendFrame(*frame);
    std::clog << "CAN_TX_RESULT status=" << operationStatusName(status) << '\n';
    return status;
}

ecOperationStatus commandDoorOpen()
{
    return ecOperationStatus::Ok;
}

ecOperationStatus commandDoorClose()
{
    return ecOperationStatus::Ok;
}

ecOperationStatus commandEmergencyStop()
{
    return ecOperationStatus::Ok;
}

} // namespace project6::supervisory::drivers
