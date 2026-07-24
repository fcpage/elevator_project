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
enum class ecRequestSource
{
    Maintenance,
    Sabbath,
    CarModule,
    FloorModule,
    WebInterface
};

/**
 * @brief One pending floor request after input normalization.
 */
struct sElevatorRequest
{
    /** Validated destination floor requested by the source module. Default value to 1. */
    std::uint8_t floor = 1;

    /** Origin category used by the scheduler's fixed priority policy. */
    ecRequestSource source = ecRequestSource::FloorModule;
};

/**
 * @brief Stores pending requests and chooses the next request to service.
 *
 * Requests remain FIFO within each source category. Selection priority is
 * the car module first, then the floor modules, then the web interface.
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

    /** Adds one internally generated Sabbath floor request. */
    void enqueueSabbathRequest(std::uint8_t floor);

    /**
     * @brief Removes the next request according to the fixed source priority.
     *
     * 1. Maintenance Mode
     * 2. Sabbath Mode
     * 3. Car Requests
     * 4. Local Network Requests
     * 5. Web Requests
     *
     * @return The selected request, or std::nullopt when all queues are empty.
     */
    std::optional<sElevatorRequest> tryTakeNextRequest();

    /**
     * @brief Removes the next request allowed by the active global mode.
     * Maintenance and Sabbath modes intentionally gate normal queues.
     *
     * @return Elevator request on success or std nullopt when all queues are empty.
     */
    std::optional<sElevatorRequest> tryTakeNextAllowedRequest(std::uint8_t modeBits);

    /** @return True when any source queue contains a request. */
    bool hasPendingRequest() const;

    /** @return True when a request from the specified queue is pending. */
    bool hasPendingRequest(ecRequestSource source) const;

    /** Removes stale requests belonging to a mode that has just been disabled. */
    void clear(ecRequestSource source);

private:
    /** FIFO maintenance requests; active only while maintenance mode is set. */
    std::deque<sElevatorRequest> maintenanceRequests_;

    /** FIFO automatic Sabbath requests; higher priority than normal service. */
    std::deque<sElevatorRequest> sabbathRequests_;

    /** FIFO in-car requests; highest scheduler priority. */
    std::deque<sElevatorRequest> carRequests_;

    /** FIFO landing-module requests; second scheduler priority. */
    std::deque<sElevatorRequest> floorRequests_;

    /** FIFO web requests; lowest scheduler priority. */
    std::deque<sElevatorRequest> webRequests_;
};

} // namespace project6::supervisory
