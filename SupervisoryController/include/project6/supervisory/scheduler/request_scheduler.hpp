/******************************************************************
* request_scheduler.hpp - Elevator Request Scheduler
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Declares request queueing and priority decisions for the supervisor.
******************************************************************/

#pragma once

#include <cstdint>
#include <deque>
#include <optional>

#include "project6/supervisory/common/event.hpp"

namespace project6::supervisory
{

/**
 * @brief Source category for a pending elevator request.
 */
enum class RequestSource
{
    FloorModule,
    CarModule,
    WebInterface
};

/**
 * @brief One pending floor request after input normalization.
 */
struct sElevatorRequest
{
    std::uint8_t floor = 1;
    RequestSource source = RequestSource::FloorModule;
};

/**
 * @brief Stores pending requests and chooses the next request to service.
 */
class cRequestScheduler
{
public:
    void enqueueEvent(const sSupervisoryEvent& event);
    std::optional<sElevatorRequest> tryTakeNextRequest();
    bool hasPendingRequest() const;

private:
    std::deque<sElevatorRequest> floorRequests_;
    std::deque<sElevatorRequest> carRequests_;
    std::deque<sElevatorRequest> webRequests_;
};

} // namespace project6::supervisory
