/******************************************************************
* supervisory_drivers.cpp - Supervisory Driver Boundary
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @brief Provides hardware-facing driver stubs used by the FSM.
******************************************************************/

#include "project6/supervisory/drivers/supervisory_drivers.hpp"

namespace project6::supervisory::drivers
{

OperationStatus commandElevatorToFloor(std::uint8_t targetFloor)
{
    static_cast<void>(targetFloor);

    return OperationStatus::Ok;
}

OperationStatus commandDoorOpen()
{
    return OperationStatus::Ok;
}

OperationStatus commandDoorClose()
{
    return OperationStatus::Ok;
}

OperationStatus commandEmergencyStop()
{
    return OperationStatus::Ok;
}

} // namespace project6::supervisory::drivers
