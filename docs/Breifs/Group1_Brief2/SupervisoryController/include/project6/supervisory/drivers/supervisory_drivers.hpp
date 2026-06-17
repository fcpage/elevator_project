/******************************************************************
* supervisory_drivers.hpp - Supervisory Driver Boundary
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Declares hardware-facing operations used by the state machine.
******************************************************************/

#pragma once

#include <cstdint>

#include "project6/supervisory/common/result.hpp"

namespace project6::supervisory
{

class cSocketCanAdapter;

namespace drivers
{

/**
 * @brief Transmits an elevator movement command over CAN.
 */
ecOperationStatus commandElevatorToFloor(
    cSocketCanAdapter& canAdapter,
    std::uint8_t targetFloor);

/**
 * @brief Requests the elevator doors to open.
 */
ecOperationStatus commandDoorOpen();

/**
 * @brief Requests the elevator doors to close.
 */
ecOperationStatus commandDoorClose();

/**
 * @brief Requests a conservative stop/fault action.
 */
ecOperationStatus commandEmergencyStop();

} // namespace drivers

} // namespace project6::supervisory
