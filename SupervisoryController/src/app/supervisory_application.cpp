/******************************************************************
* supervisory_application.cpp - Supervisory Application Orchestrator
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Provides the top-level multithreaded loop for the SC.
******************************************************************/

#include "supervisory/app/supervisory_application.hpp"

#include <cstddef>
#include <iostream>

namespace project6::supervisory
{

namespace
{

constexpr std::size_t kMaximumEventsPerCycle = 16;
constexpr std::chrono::milliseconds kCommsHeartbeatTimeout{250};

} // namespace

cSupervisoryApplication::cSupervisoryApplication(sCanExchange& exchange)
    : exchange_(exchange)
{
}

ecOperationStatus cSupervisoryApplication::runControlCycle(
    const std::chrono::milliseconds elapsedMs)
{
    checkCommsHealth(elapsedMs);

    for (std::size_t count = 0; count < kMaximumEventsPerCycle; ++count)
    {
        sSupervisoryEvent event{};
        if (!exchange_.receivedEvents.tryPop(event))
        {
            break;
        }
        appStateMachine_.handleEvent(event);
        publishPendingFrame();
    }

    sSupervisoryEvent timerEvent{};
    timerEvent.type = ecEventType::TimerTick;
    timerEvent.timestampMs = elapsedMs;
    appStateMachine_.handleEvent(timerEvent);
    publishPendingFrame();

    return ecOperationStatus::Ok;
}

sSupervisoryStateSnapshot cSupervisoryApplication::snapshot() const
{
    return appStateMachine_.snapshot();
}

sCanCommsHealthSnapshot cSupervisoryApplication::canHealth() const
{
    return {
        exchange_.commsState.load(),
        exchange_.faultReason.load(),
        exchange_.heartbeat.load(),
        exchange_.receivedFrameCount.load(),
        exchange_.droppedEventCount.load(),
        exchange_.transmittedFrameCount.load(),
        exchange_.transmitFailureCount.load()};
}

void cSupervisoryApplication::publishPendingFrame()
{
    if (const std::optional<sCanFrame> frame = appStateMachine_.tryTakePendingCanFrame();
        frame.has_value() && !exchange_.transmitFrames.tryPush(*frame))
    {
        faultComms(ecCanCommsFaultReason::OutboundQueueFull);
    }
}

void cSupervisoryApplication::checkCommsHealth(
    const std::chrono::milliseconds elapsedMs)
{
    // Duplicate event. Return.
    if (isCommsFaultLatched_)
    {
        return;
    }

    if (const std::uint64_t heartbeat = exchange_.heartbeat.load(); heartbeat != lastHeartbeat_)
    {
        lastHeartbeat_ = heartbeat;
        staleHeartbeatElapsed_ = std::chrono::milliseconds{0};
    }
    else if (exchange_.commsState.load() == ecCanCommsState::Running)
    {
        staleHeartbeatElapsed_ += elapsedMs;
    }

    const std::uint64_t droppedEvents = exchange_.droppedEventCount.load();
    const std::uint64_t transmitFailures = exchange_.transmitFailureCount.load();
    const bool didDropEvent = droppedEvents != lastDroppedEventCount_;
    const bool didTransmitFail = transmitFailures != lastTransmitFailureCount_;

    lastDroppedEventCount_ = droppedEvents;
    lastTransmitFailureCount_ = transmitFailures;

    if (didDropEvent)
    {
        faultComms(ecCanCommsFaultReason::InboundQueueFull);
    }
    else if (didTransmitFail)
    {
        faultComms(ecCanCommsFaultReason::TransmitFailed);
    }
    else if (exchange_.commsState.load() == ecCanCommsState::Failed)
    {
        const ecCanCommsFaultReason reason = exchange_.faultReason.load();
        faultComms(
            reason == ecCanCommsFaultReason::None
                ? ecCanCommsFaultReason::ThreadFailed
                : reason);
    }
    else if (staleHeartbeatElapsed_ >= kCommsHeartbeatTimeout)
    {
        faultComms(ecCanCommsFaultReason::HeartbeatTimeout);
    }
}

void cSupervisoryApplication::faultComms(const ecCanCommsFaultReason reason)
{
    if (isCommsFaultLatched_)
    {
        return;
    }

    ecCanCommsFaultReason expected = ecCanCommsFaultReason::None;
    static_cast<void>(exchange_.faultReason.compare_exchange_strong(expected, reason));
    isCommsFaultLatched_ = true;

    // Try and restart with default configuration.
    const sSocketCanConfig canConfig;
    sCanExchange canExchange;
    cCanCommsService commsService(canConfig, canExchange);
    cSupervisoryApplication application(canExchange);

    if (const ecOperationStatus status = commsService.initializeService(); status != ecOperationStatus::Ok)
    {
        std::cerr << "supervisory_controller: COMMS restart initialization failed." << std::endl;
    }

    if (const ecOperationStatus status = commsService.start(); status != ecOperationStatus::Ok)
    {
        std::cerr << "supervisory_controller: COMMS restart failed." << std::endl;
    }
    else {
        std::clog << "supervisory_controller: Recovery successful. COMMS restarted." << std::endl;
        return;
    }

    sSupervisoryEvent event{};
    event.type = ecEventType::Fault;
    appStateMachine_.handleEvent(event);
}

}
