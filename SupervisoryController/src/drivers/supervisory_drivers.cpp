/******************************************************************
* supervisory_drivers.cpp - Supervisory Driver Boundary
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Provides hardware-facing operations used by the state machine.
******************************************************************/

#include "supervisory/drivers/supervisory_drivers.hpp"

namespace project6::supervisory::drivers
{

    // change to say arrive command -> floor controllers turn on light and open door
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
