/******************************************************************
* event.hpp - Supervisory Event Definitions
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @file event.hpp
* @brief Defines normalized events consumed by the supervisory controller.
******************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace project6::supervisory
{

/**
 * @brief Identifies the source or purpose of an event entering the supervisor.
 */
enum class ecEventType
{
    /** Floor request received from the local web interface. */
    HttpFloorRequest,

    /** Landing request received from a floor controller over CAN. */
    CanFloorRequest,

    /** In-car floor request received over CAN. */
    CanCarRequest,

    /** Elevator position/status report received over CAN. */
    CanElevatorStatus,

    /** Elapsed monotonic time supplied by the application loop. */
    TimerTick,

    /** Fault indication that latches the machine into its safe state. */
    Fault
};

/**
 * @brief Describes an elevator direction using controller-level language.
 */
enum class ecTravelDirection
{
    None,
    Up,
    Down
};

/**
 * @brief Normalized event passed from adapters into the supervisor.
 *
 * Producers populate only the fields meaningful for the selected type. This
 * keeps transport details out of the state machine while allowing CAN, HTTP,
 * and timer inputs to share one event-processing path.
 */
struct sSupervisoryEvent
{
    /** Selects how the remaining event fields are interpreted. */
    ecEventType type;

    /** Destination floor for HTTP, landing, or in-car request events. */
    std::optional<std::uint8_t> requestedFloor;

    /** Confirmed elevator floor carried by a CAN status event. */
    std::optional<std::uint8_t> reportedFloor;

    /** Reported travel direction when the source protocol provides one. */
    ecTravelDirection reportedDirection = ecTravelDirection::None;

    /** Elapsed duration carried by TimerTick events. */
    std::chrono::milliseconds timestampMs{0};
};

} // namespace project6::supervisory
