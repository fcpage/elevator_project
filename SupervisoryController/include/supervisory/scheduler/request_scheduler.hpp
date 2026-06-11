/******************************************************************
* request_scheduler.hpp - Elevator Request Scheduler
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @file request_scheduler.hpp
* @brief Declares request queueing and priority decisions for the supervisor.
******************************************************************/

#pragma once

#include <cstdint>
#include <deque>
#include <optional>

#include "supervisory/common/event.hpp"

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
    /** Validated destination floor requested by the source module. */
    std::uint8_t floor = 1;

    /** Origin category used by the scheduler's fixed priority policy. */
    RequestSource source = RequestSource::FloorModule;
};

/**
 * @brief Stores pending requests and chooses the next request to service.
 *
 * Requests remain FIFO within each source category. Selection priority is
 * landing modules first, then the car module, then the web interface.
 */
class cRequestScheduler
{
public:
    /**
     * @brief Converts a request event into its source-specific FIFO queue.
     *
     * Events without a requested floor, and non-request event types, are ignored.
     *
     * @param event Normalized request event from an input adapter.
     */
    void enqueueEvent(const sSupervisoryEvent& event);

    /**
     * @brief Removes the next request according to the fixed source priority.
     * 1. Car Requests
     * 2. Local Network Requests
     * 3. Web Requests
     *
     * @return The selected request, or std::nullopt when all queues are empty.
     */
    std::optional<sElevatorRequest> tryTakeNextRequest();

    /** @return True when any source queue contains a request. */
    bool hasPendingRequest() const;

private:
    /** FIFO in-car requests; highest scheduler priority. */
    std::deque<sElevatorRequest> carRequests_;

    /** FIFO landing-module requests; second scheduler priority. */
    std::deque<sElevatorRequest> floorRequests_;

    /** FIFO web requests; lowest scheduler priority. */
    std::deque<sElevatorRequest> webRequests_;
};

} // namespace project6::supervisory
