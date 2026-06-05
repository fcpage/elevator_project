/******************************************************************
* event.hpp - Supervisory Event Definitions
* Author: Project 6 Team
* Last Modified: 2026-05-31
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
enum class EventType
{
    HttpFloorRequest,
    CanFloorRequest,
    CanCarRequest,
    CanElevatorStatus,
    TimerTick,
    Fault
};

/**
 * @brief Describes an elevator direction using controller-level language.
 */
enum class TravelDirection
{
    None,
    Up,
    Down
};

/**
 * @brief Normalized event passed from adapters into the supervisor.
 */
struct SupervisoryEvent
{
    EventType type = EventType::TimerTick;
    std::optional<std::uint8_t> requestedFloor;
    std::optional<std::uint8_t> reportedFloor;
    TravelDirection reportedDirection = TravelDirection::None;
    std::chrono::milliseconds timestampMs{0};
};

} // namespace project6::supervisory
