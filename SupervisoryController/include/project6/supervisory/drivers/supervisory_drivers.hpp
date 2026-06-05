/******************************************************************
* supervisory_drivers.hpp - Supervisory Driver Stub Boundary
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Declares temporary hardware-facing driver stubs used by the FSM.
******************************************************************/

#pragma once

#include <cstdint>

#include "project6/supervisory/common/result.hpp"

namespace project6::supervisory::drivers
{

/**
 * @brief Requests elevator travel to a validated target floor.
 */
OperationStatus commandElevatorToFloor(std::uint8_t targetFloor);

/**
 * @brief Requests the elevator doors to open.
 */
OperationStatus commandDoorOpen();

/**
 * @brief Requests the elevator doors to close.
 */
OperationStatus commandDoorClose();

/**
 * @brief Requests a conservative stop/fault action.
 */
OperationStatus commandEmergencyStop();

} // namespace project6::supervisory::drivers
