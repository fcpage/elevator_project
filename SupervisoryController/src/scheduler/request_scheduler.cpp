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
    if (!event.requestedFloor.has_value())
    {
        return;
    }

    sElevatorRequest request{};
    request.floor = *event.requestedFloor;

    switch (event.type)
    {
        case ecEventType::CanCarRequest:
        {
            request.source = RequestSource::CarModule;
            carRequests_.push_back(request);
            break;
        }

        case ecEventType::CanFloorRequest:
        {
            request.source = RequestSource::FloorModule;
            floorRequests_.push_back(request);
            break;
        }

        case ecEventType::HttpFloorRequest:
        {
            request.source = RequestSource::WebInterface;
            webRequests_.push_back(request);
            break;
        }

        default:
        {
            break;
        }
    }
}

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

bool cRequestScheduler::hasPendingRequest() const
{
    return !floorRequests_.empty() || !carRequests_.empty() || !webRequests_.empty();
}

} // namespace project6::supervisory
