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
struct ElevatorRequest
{
    std::uint8_t floor = 1;
    RequestSource source = RequestSource::FloorModule;
};

/**
 * @brief Stores pending requests and chooses the next request to service.
 *
 * Start simple. The first real version can prioritize physical floor requests,
 * then car requests, then web requests. Do not let this class command hardware.
 */
class RequestScheduler
{
public:
    void enqueueEvent(const SupervisoryEvent& event);
    std::optional<ElevatorRequest> tryTakeNextRequest();
    bool hasPendingRequest() const;

private:
    std::deque<ElevatorRequest> floorRequests_;
    std::deque<ElevatorRequest> carRequests_;
    std::deque<ElevatorRequest> webRequests_;
};

} // namespace project6::supervisory
