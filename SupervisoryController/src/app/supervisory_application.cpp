/******************************************************************
* supervisory_application.cpp - Supervisory Application Orchestrator
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Provides the top-level polling loop for the controller.
******************************************************************/

#include "project6/supervisory/app/supervisory_application.hpp"

#include "project6/supervisory/can/can_protocol.hpp"

namespace project6::supervisory
{

SupervisoryApplication::SupervisoryApplication(SocketCanAdapter& canAdapter, HttpServer& httpServer)
    : canAdapter_(canAdapter),
      httpServer_(httpServer)
{
} // namespace project6::supervisory

OperationStatus SupervisoryApplication::initialize()
{
    const OperationStatus canStatus = canAdapter_.initialize();
    if (canStatus != OperationStatus::Ok)
    {
        return canStatus;
    }

    const OperationStatus httpStatus = httpServer_.initialize();
    if (httpStatus != OperationStatus::Ok)
    {
        return httpStatus;
    }

    isInitialized_ = true;
    return OperationStatus::Ok;
}

OperationStatus SupervisoryApplication::runOnce(std::chrono::milliseconds elapsedMs)
{
    if (!isInitialized_)
    {
        return OperationStatus::NotInitialized;
    }

    pollCan();
    pollHttp();
    processTimer(elapsedMs);

    return OperationStatus::Ok;
}

void SupervisoryApplication::pollCan()
{
    const std::optional<CanFrame> frame = canAdapter_.tryReadFrame();
    if (!frame.has_value())
    {
        return;
    }

    const std::optional<DecodedCanMessage> message = decodeCanFrame(*frame);
    if (!message.has_value())
    {
        return;
    }

    const std::optional<SupervisoryEvent> event = toSupervisoryEvent(*message);
    if (!event.has_value())
    {
        return;
    }

    stateMachine_.handleEvent(*event);
}

void SupervisoryApplication::pollHttp()
{
    const std::optional<SupervisoryEvent> event = httpServer_.tryReadEvent();
    if (!event.has_value())
    {
        return;
    }

    stateMachine_.handleEvent(*event);
}

void SupervisoryApplication::processTimer(std::chrono::milliseconds elapsedMs)
{
    SupervisoryEvent event{};
    event.type = EventType::TimerTick;
    event.timestampMs = elapsedMs;

    stateMachine_.handleEvent(event);
}

}
