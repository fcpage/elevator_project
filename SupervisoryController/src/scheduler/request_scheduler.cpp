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
        case ecEventType::CanCarRequest:
        {
            request.source = RequestSource::CarModule;
            carRequests_.push_back(request);            // Request scheduler class private member for car requests
            break;
        }

        case ecEventType::CanFloorRequest:
        {
            request.source = RequestSource::FloorModule;
            floorRequests_.push_back(request);          // Request scheduler class private member for floor requests
            break;
        }

        case ecEventType::HttpFloorRequest:
        {
            request.source = RequestSource::WebInterface;
            webRequests_.push_back(request);            // Request scheduler class private member for web requests
            break;
        }

        default:
        {
            break;
        }
    }
}

// Optional return either an Elevator request struct or a nullopt on failure
std::optional<sElevatorRequest> cRequestScheduler::tryTakeNextRequest()
{
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

// Trailing "func() const" -> Means the function is a read-only member function.
// This function promises it will not alter any non-static members of the class.
// It also will not call any non-constant functions. Basically: No touchy!
bool cRequestScheduler::hasPendingRequest() const
{
    return !floorRequests_.empty() || !carRequests_.empty() || !webRequests_.empty();
}

} // namespace project6::supervisory
