/******************************************************************
* supervisory_application.cpp - Supervisory Application Orchestrator
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Provides the top-level multithreaded loop for the SC.
******************************************************************/

#include "supervisory/app/supervisory_application.hpp"

#include <cstddef>
#include <iostream>
#include <optional>

namespace project6::supervisory
{

namespace
{

constexpr std::size_t kMaximumEventsPerCycle = 16;
constexpr std::chrono::milliseconds kCommsProgressTimeout{250};
constexpr std::chrono::seconds kNodeHbInterval{4};
constexpr std::chrono::seconds kNodeHbReplyWindow{3};

std::optional<std::uint8_t> nodeHbMaskFromSourceId(const std::uint16_t sourceId)
{
    switch (sourceId)
    {
        case kCarControllerCanId:
            return kNodeHbCcMask;

        case kFloorOneControllerCanId:
            return kNodeHbFc1Mask;

        case kFloorTwoControllerCanId:
            return kNodeHbFc2Mask;

        case kFloorThreeControllerCanId:
            return kNodeHbFc3Mask;

        default:
            return std::nullopt;
    }
}

void logMissedNodeHbReplies(const std::uint8_t missedMask)
{
    std::cerr << "NODE_HB_TIMEOUT missing_mask=0x"
              << std::hex << static_cast<unsigned int>(missedMask)
              << std::dec;

    if ((missedMask & kNodeHbCcMask) != 0)
    {
        std::cerr << " CC";
    }
    if ((missedMask & kNodeHbFc1Mask) != 0)
    {
        std::cerr << " FC1";
    }
    if ((missedMask & kNodeHbFc2Mask) != 0)
    {
        std::cerr << " FC2";
    }
    if ((missedMask & kNodeHbFc3Mask) != 0)
    {
        std::cerr << " FC3";
    }

    std::cerr << '\n';
}

void logNodeHbError(const std::uint16_t sourceId, const std::uint8_t nodeMask)
{
    std::cerr << "NODE_HB_ERROR source_id=0x"
              << std::hex << static_cast<unsigned int>(sourceId)
              << " node_mask=0x" << static_cast<unsigned int>(nodeMask)
              << std::dec << '\n';
}

} // namespace

cSupervisoryApplication::cSupervisoryApplication(
    sCanExchange& canExchange,
    sDBMessageExchange& databaseExchange,
    const ecNodeHbFailureMode nodeHbFailureMode)
    : canExchange_(canExchange), databaseExchange_(databaseExchange), 
    nodeHbFailureMode_(nodeHbFailureMode)
{
}

// Main Loop
ecOperationStatus cSupervisoryApplication::runControlCycle(
    const std::chrono::milliseconds elapsedMs)
{
    checkCommsHealth(elapsedMs);

    // Drain can queue
    for (std::size_t count = 0; count < kMaximumEventsPerCycle; ++count)
    {
        sSupervisoryEvent event{};
        if (!canExchange_.receivedEvents.tryPop(event))
        {
            break;
        }
        appStateMachine_.handleEvent(event);
        publishPendingFrame();
    }

    // Drain database queue
    for (std::size_t count = 0; count < kMaximumEventsPerCycle; ++count)
    {
        sSupervisoryEvent event{};
        if (!databaseExchange_.readEvents.tryPop(event))
        {
            break;
        }
        appStateMachine_.handleEvent(event);
        publishSnapshot();
    }

    sSupervisoryEvent timerEvent{};
    timerEvent.type = ecEventType::TimerTick;
    timerEvent.timestampMs = elapsedMs;
    appStateMachine_.handleEvent(timerEvent);
    publishPendingFrame();

    processNodeHbCycle(elapsedMs);

    return ecOperationStatus::Ok;
}

sSupervisoryStateSnapshot cSupervisoryApplication::snapshot() const
{
    return appStateMachine_.snapshot();
}

sCanCommsHealthSnapshot cSupervisoryApplication::canHealth() const
{
    return {
        canExchange_.commsState.load(),
        canExchange_.faultReason.load(),
        canExchange_.commsProgress.load(),
        canExchange_.receivedFrameCount.load(),
        canExchange_.droppedEventCount.load(),
        canExchange_.transmittedFrameCount.load(),
        canExchange_.transmitFailureCount.load(),
        expectedNodeHbReplyMask_,
        receivedNodeHbReplyMask_,
        missedNodeHbReplyMask_,
        isNodeHbReplyWindowOpen_};
}

void cSupervisoryApplication::publishPendingFrame()
{
    // If we have a frame, and we cannot push that frame to the transmit queue fault.
    while (true)
    {
        const std::optional<sCanFrame> frame = appStateMachine_.tryTakePendingCanFrame();
        if (!frame.has_value())
        {
            return;
        }

        if (!canExchange_.transmitFrames.tryPush(*frame))
        {
            faultComms(ecCanCommsFaultReason::OutboundQueueFull);
            return;
        }
    }
}

void cSupervisoryApplication::publishSnapshot() {
    if (const std::optional<sSupervisoryStateSnapshot> snapshot = appStateMachine_.snapshot();
        snapshot.has_value() && !databaseExchange_.writableSnapshots.tryPush(*snapshot))
    {
        std::cerr << "ERROR: Supervisory snapshot not writable to database." << std::endl;
    }
}

void cSupervisoryApplication::checkCommsHealth(
    const std::chrono::milliseconds elapsedMs)
{
    // Duplicate event. Return.
    if (isControlFaultLatched_)
    {
        return;
    }

    // Reset stale progress counter if the progress counter has changed since last check
    if (const std::uint64_t progress = canExchange_.commsProgress.load(); progress != lastCommsProgress_)
    {
        lastCommsProgress_ = progress;
        staleCommsProgressElapsed_ = std::chrono::milliseconds{0};
    }
    // Else if it is the same value the comms are hanging, add the elapsed ms.
    else if (canExchange_.commsState.load() == ecCanCommsState::Running)
    {
        staleCommsProgressElapsed_ += elapsedMs;
    }

    // Check for standard comm failures
    const std::uint64_t droppedEvents = canExchange_.droppedEventCount.load();
    const std::uint64_t transmitFailures = canExchange_.transmitFailureCount.load();
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
    else if (canExchange_.commsState.load() == ecCanCommsState::Failed)
    {
        const ecCanCommsFaultReason reason = canExchange_.faultReason.load();
        faultComms(
            reason == ecCanCommsFaultReason::None
                ? ecCanCommsFaultReason::ThreadFailed
                : reason);
    }
    else if (staleCommsProgressElapsed_ >= kCommsProgressTimeout)
    {
        faultComms(ecCanCommsFaultReason::CommsProgressTimeout);
    }
}

void cSupervisoryApplication::processNodeHbCycle(
    const std::chrono::milliseconds elapsedMs)
{
    sNodeHbMessage message{};

    // Pull received HB messages from the SPSC queue
    while (canExchange_.receivedNodeHbMessages.tryPop(message))
    {
        handleNodeHbMessage(message);
    }

    if (isNodeHbReplyWindowOpen_)
    {
        nodeHbReplyWindowElapsed_ += elapsedMs;
        if (nodeHbReplyWindowElapsed_ >= kNodeHbReplyWindow)
        {
            reportNodeHbTimeout();
        }
    }

    nodeHbIntervalElapsed_ += elapsedMs;
    if (nodeHbIntervalElapsed_ < kNodeHbInterval)
    {
        return;
    }

    nodeHbIntervalElapsed_ -= kNodeHbInterval;
    if (!isNodeHbReplyWindowOpen_)
    {
        startNodeHbRequest();
    }
}

//
void cSupervisoryApplication::handleNodeHbMessage(const sNodeHbMessage& message)
{
    if (message.type == ecNodeHb::NodeRequest)
    {
        publishNodeHbFrame(ecNodeHb::Ok);
        return;
    }

    if (message.type == ecNodeHb::Error)
    {
        reportNodeHbError(message.sourceId);
        return;
    }

    if (message.type != ecNodeHb::Ok)
    {
        return;
    }

    const std::optional<std::uint8_t> replyMask = nodeHbMaskFromSourceId(message.sourceId);
    if (!replyMask.has_value())
    {
        return;
    }

    receivedNodeHbReplyMask_ |= *replyMask;
    missedNodeHbReplyMask_ = 0;

    if ((receivedNodeHbReplyMask_ & expectedNodeHbReplyMask_) == expectedNodeHbReplyMask_)
    {
        isNodeHbReplyWindowOpen_ = false;
        nodeHbReplyWindowElapsed_ = std::chrono::milliseconds{0};
    }
}

// Publish a HB request to the CAN bus
void cSupervisoryApplication::startNodeHbRequest()
{
    expectedNodeHbReplyMask_ = kExpectedNodeHbReplyMask;
    receivedNodeHbReplyMask_ = 0;
    missedNodeHbReplyMask_ = 0;
    nodeHbReplyWindowElapsed_ = std::chrono::milliseconds{0};
    isNodeHbReplyWindowOpen_ = true;

    publishNodeHbFrame(ecNodeHb::SupervisorRequest);
}

void cSupervisoryApplication::publishNodeHbFrame(const ecNodeHb type)
{
    // If we have Hb frames to transmit and they fail to push to the outbound queue fault
    if (const std::optional<sCanFrame> frame = makeNodeHbFrame(kSupervisoryControllerCanId, type);
        frame.has_value() && !canExchange_.transmitFrames.tryPush(*frame))
    {
        faultComms(ecCanCommsFaultReason::OutboundQueueFull);
    }
}

void cSupervisoryApplication::reportNodeHbError(const std::uint16_t sourceId)
{
    const std::optional<std::uint8_t> nodeMask = nodeHbMaskFromSourceId(sourceId);
    missedNodeHbReplyMask_ = nodeMask.value_or(0);
    isNodeHbReplyWindowOpen_ = false;
    nodeHbReplyWindowElapsed_ = std::chrono::milliseconds{0};

    logNodeHbError(sourceId, missedNodeHbReplyMask_);
    if (nodeHbFailureMode_ == ecNodeHbFailureMode::FaultControl)
    {
        faultControl(ecCanCommsFaultReason::NodeHeartbeatError);
    }
}

void cSupervisoryApplication::reportNodeHbTimeout()
{
    missedNodeHbReplyMask_ =
        static_cast<std::uint8_t>(expectedNodeHbReplyMask_ & ~receivedNodeHbReplyMask_);
    isNodeHbReplyWindowOpen_ = false;
    nodeHbReplyWindowElapsed_ = std::chrono::milliseconds{0};

    if (missedNodeHbReplyMask_ == 0)
    {
        return;
    }

    logMissedNodeHbReplies(missedNodeHbReplyMask_);
    if (nodeHbFailureMode_ == ecNodeHbFailureMode::FaultControl)
    {
        faultControl(ecCanCommsFaultReason::NodeHeartbeatTimeout);
    }
}

void cSupervisoryApplication::faultControl(const ecCanCommsFaultReason reason)
{
    if (isControlFaultLatched_)
    {
        return;
    }

    ecCanCommsFaultReason expected = ecCanCommsFaultReason::None;

    /****************************************************************************************
     * Note to other contributors:
     *  compare_exchange_strong is an atomic read-modify-write operation used for lock-free
     *  syncrhronization. If the compared values match, the function overwrites the atomic val
     *  with the desired val. If they're not equal, the "expected" value is overwritten with the
     *  actual atomic value.
     *
     *  We're using the strong variant over the weak variant to protect against spurious failures.
     *  Note: it compares the object representation (until C++20, and value representation after).
    *******************************************************************************************/
    static_cast<void>(canExchange_.faultReason.compare_exchange_strong(expected, reason));
    isControlFaultLatched_ = true;

    sSupervisoryEvent event{};
    event.type = ecEventType::Fault;
    appStateMachine_.handleEvent(event);
}

void cSupervisoryApplication::faultComms(const ecCanCommsFaultReason reason)
{
    if (isControlFaultLatched_)
    {
        return;
    }

    ecCanCommsFaultReason expected = ecCanCommsFaultReason::None;
    static_cast<void>(canExchange_.faultReason.compare_exchange_strong(expected, reason));
    isControlFaultLatched_ = true;

    // Try and restart with default configuration.
    const sSocketCanConfig canConfig;
    sCanExchange canExchange;
    cCanCommsService commsService(canConfig, canExchange);

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
