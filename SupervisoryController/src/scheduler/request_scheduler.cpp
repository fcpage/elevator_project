/******************************************************************
* request_scheduler.cpp - Elevator Request Scheduler
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @brief Provides request-priority handling for supervisor scheduling.
******************************************************************/

#include "supervisory/scheduler/request_scheduler.hpp"

namespace project6::supervisory
{

void cRequestScheduler::enqueueEvent(const sSupervisoryEvent& event)
{
    // Return if there is no requested floor
    if (!event.requestedFloor.has_value())
    {
        return;
    }

    // Struct declaration
    sElevatorRequest request{};
    request.floor = *event.requestedFloor; // Get floor request from event

    switch (event.type)
    {
        case ecEventType::MaintenanceFloorRequest:
        {
            request.source = ecRequestSource::Maintenance;
            maintenanceRequests_.push_back(request);
            break;
        }

        case ecEventType::CanCarRequest:
        {
            request.source = ecRequestSource::CarModule;
            carRequests_.push_back(request);            // Request scheduler class private member for car requests
            break;
        }

        case ecEventType::CanFloorRequest:
        {
            request.source = ecRequestSource::FloorModule;
            floorRequests_.push_back(request);          // Request scheduler class private member for floor requests
            break;
        }

        case ecEventType::HttpFloorRequest:
        {
            request.source = ecRequestSource::WebInterface;
            webRequests_.push_back(request);            // Request scheduler class private member for web requests
            break;
        }

        default:
        {
            break;
        }
    }
}

void cRequestScheduler::enqueueSabbathRequest(const std::uint8_t floor)
{
    sabbathRequests_.push_back({floor, ecRequestSource::Sabbath});
}

// Optional return either an Elevator request struct or a nullopt on failure
std::optional<sElevatorRequest> cRequestScheduler::tryTakeNextRequest()
{
    if (!maintenanceRequests_.empty())
    {
        sElevatorRequest request = maintenanceRequests_.front();
        maintenanceRequests_.pop_front();
        return request;
    }

    if (!sabbathRequests_.empty())
    {
        sElevatorRequest request = sabbathRequests_.front();
        sabbathRequests_.pop_front();
        return request;
    }

    if (!carRequests_.empty())
    {
        sElevatorRequest request = carRequests_.front();
        carRequests_.pop_front();
        return request;
    }

    if (!floorRequests_.empty())
    {
        sElevatorRequest request = floorRequests_.front();
        floorRequests_.pop_front();
        return request;
    }

    if (!webRequests_.empty())
    {
        sElevatorRequest request = webRequests_.front();
        webRequests_.pop_front();
        return request;
    }

    return std::nullopt;
}

std::optional<sElevatorRequest> cRequestScheduler::tryTakeNextAllowedRequest(
    const std::uint8_t modeBits)
{
    if ((modeBits & kModeMaintenance) != 0)
    {
        if (!maintenanceRequests_.empty())
        {
            const sElevatorRequest request = maintenanceRequests_.front();
            maintenanceRequests_.pop_front();
            return request;
        }
        return std::nullopt;
    }

    if ((modeBits & kModeSabbath) != 0)
    {
        if (!sabbathRequests_.empty())
        {
            const sElevatorRequest request = sabbathRequests_.front();
            sabbathRequests_.pop_front();
            return request;
        }
        return std::nullopt;
    }

    // Mode-specific queues are intentionally not eligible when their mode bit
    // is clear. This also protects against a request arriving just before a
    // GUI/database mode-clear event is applied.
    if (!carRequests_.empty())
    {
        const sElevatorRequest request = carRequests_.front();
        carRequests_.pop_front();
        return request;
    }
    if (!floorRequests_.empty())
    {
        const sElevatorRequest request = floorRequests_.front();
        floorRequests_.pop_front();
        return request;
    }
    if (!webRequests_.empty())
    {
        const sElevatorRequest request = webRequests_.front();
        webRequests_.pop_front();
        return request;
    }

    return std::nullopt;
}

// Trailing "func() const" -> Means the function is a read-only member function.
// This function promises it will not alter any non-static members of the class.
// It also will not call any non-constant functions. Basically: No touchy!
bool cRequestScheduler::hasPendingRequest() const
{
    return !maintenanceRequests_.empty() || !sabbathRequests_.empty() ||
           !floorRequests_.empty() || !carRequests_.empty() || !webRequests_.empty();
}

bool cRequestScheduler::hasPendingRequest(const ecRequestSource source) const
{
    switch (source)
    {
        case ecRequestSource::Maintenance:
            return !maintenanceRequests_.empty();
        case ecRequestSource::Sabbath:
            return !sabbathRequests_.empty();
        case ecRequestSource::CarModule:
            return !carRequests_.empty();
        case ecRequestSource::FloorModule:
            return !floorRequests_.empty();
        case ecRequestSource::WebInterface:
            return !webRequests_.empty();
    }

    return false;
}

void cRequestScheduler::clear(const ecRequestSource source)
{
    switch (source)
    {
        case ecRequestSource::Maintenance:
            maintenanceRequests_.clear();
            break;
        case ecRequestSource::Sabbath:
            sabbathRequests_.clear();
            break;
        case ecRequestSource::CarModule:
            carRequests_.clear();
            break;
        case ecRequestSource::FloorModule:
            floorRequests_.clear();
            break;
        case ecRequestSource::WebInterface:
            webRequests_.clear();
            break;
    }
}

} // namespace project6::supervisory
