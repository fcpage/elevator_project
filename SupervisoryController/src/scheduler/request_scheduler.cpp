/******************************************************************
* request_scheduler.cpp - Elevator Request Scheduler
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @brief Provides request-priority handling for supervisor scheduling.
******************************************************************/

#include "project6/supervisory/scheduler/request_scheduler.hpp"

namespace project6::supervisory
{

void RequestScheduler::enqueueEvent(const SupervisoryEvent& event)
{
    if (!event.requestedFloor.has_value())
    {
        return;
    }

    ElevatorRequest request{};
    request.floor = *event.requestedFloor;

    switch (event.type)
    {
        case EventType::CanFloorRequest:
        {
            request.source = RequestSource::FloorModule;
            floorRequests_.push_back(request);
            break;
        }

        case EventType::CanCarRequest:
        {
            request.source = RequestSource::CarModule;
            carRequests_.push_back(request);
            break;
        }

        case EventType::HttpFloorRequest:
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

std::optional<ElevatorRequest> RequestScheduler::tryTakeNextRequest()
{
    if (!floorRequests_.empty())
    {
        ElevatorRequest request = floorRequests_.front();
        floorRequests_.pop_front();
        return request;
    }

    if (!carRequests_.empty())
    {
        ElevatorRequest request = carRequests_.front();
        carRequests_.pop_front();
        return request;
    }

    if (!webRequests_.empty())
    {
        ElevatorRequest request = webRequests_.front();
        webRequests_.pop_front();
        return request;
    }

    return std::nullopt;
}

bool RequestScheduler::hasPendingRequest() const
{
    return !floorRequests_.empty() || !carRequests_.empty() || !webRequests_.empty();
}

} // namespace project6::supervisory
