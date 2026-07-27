/******************************************************************
* supervisory_application.cpp - Supervisory Application Orchestrator
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Provides the top-level multithreaded loop for the SC.
******************************************************************/

#include "supervisory/app/supervisory_application.hpp"

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <optional>

namespace project6::supervisory
{

namespace
{

constexpr std::size_t kMaximumEventsPerCycle = 16;
constexpr std::chrono::milliseconds kCommsProgressTimeout{250};
// Keep these values aligned with the rev3 FSM design and control-loop tests:
// a supervisory request is sent every 3 seconds and nodes have 1 second to
// complete the reply window. This was already a dev-branch mismatch.
constexpr std::chrono::seconds kNodeHbInterval{3};
constexpr std::chrono::seconds kNodeHbReplyWindow{1};

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
    sCanExchange& exchange,
    const ecNodeHbFailureMode nodeHbFailureMode,
    cAnnouncementService* announcementService)
    : exchange_(exchange), announcementService_(announcementService), nodeHbFailureMode_(nodeHbFailureMode)
{
}

bool cSupervisoryApplication::enqueueAdapterEvent(const sSupervisoryEvent& event)
{
    return adapterEvents_.tryPush(event);
}

void cSupervisoryApplication::setSabbathStopDuration(
    const std::chrono::milliseconds duration)
{
    appStateMachine_.setSabbathStopDuration(duration);
}

// Main Loop
ecOperationStatus cSupervisoryApplication::runControlCycle(
    const std::chrono::milliseconds elapsedMs)
{
    writeCanLogRecords();
    checkCommsHealth(elapsedMs);

    for (std::size_t count = 0; count < kMaximumEventsPerCycle; ++count)
    {
        sSupervisoryEvent event{};
        if (adapterEvents_.tryPop(event))
        {
            processControlEvent(event);
            continue;
        }
        if (!exchange_.receivedEvents.tryPop(event))
        {
            break;
        }
        processControlEvent(event);
    }

    sSupervisoryEvent timerEvent{};
    timerEvent.type = ecEventType::TimerTick;
    timerEvent.timestampMs = elapsedMs;
    processControlEvent(timerEvent);

    processNodeHbCycle(elapsedMs);

    return ecOperationStatus::Ok;
}

void cSupervisoryApplication::writeCanLogRecords()
{
    // Temporary database seam: replace this file append with an INSERT/batch
    // writer for the database CAN-log table here, while keeping this queue as
    // the COMMS-to-CONTROL handoff.
    if (!canLogFile_.is_open())
    {
        return;
    }

    sCanLogRecord record{};
    while (exchange_.canLogRecords.tryPop(record))
    {
        canLogFile_ << "timestamp_ms=" << record.timestampMs
                    << " direction="
                    << (record.direction == ecCanLogDirection::Received ? "rx" : "tx")
                    << " id=0x" << std::hex << record.frame.id << std::dec
                    << " dlc=" << static_cast<unsigned int>(record.frame.dataLength)
                    << " data=";
        for (std::uint8_t index = 0; index < record.frame.dataLength; ++index)
        {
            canLogFile_ << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned int>(record.frame.data[index]);
        }
        canLogFile_ << std::setfill(' ') << std::dec << '\n';
    }
    canLogFile_.flush();
}

void cSupervisoryApplication::processControlEvent(const sSupervisoryEvent& event)
{
    const sSupervisoryStateSnapshot before = appStateMachine_.snapshot();
    appStateMachine_.handleEvent(event);
    publishPendingFrame();
    publishArrivalAnnouncement(before);
}

void cSupervisoryApplication::publishArrivalAnnouncement(
    const sSupervisoryStateSnapshot& before)
{
    if (announcementService_ == nullptr)
    {
        return;
    }

    const sSupervisoryStateSnapshot after = appStateMachine_.snapshot();
    if (before.controlState != ecSupervisoryControlState::Arrived &&
        after.controlState == ecSupervisoryControlState::Arrived)
    {
        static_cast<void>(announcementService_->submit(after.currentFloor));
    }
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
        exchange_.commsProgress.load(),
        exchange_.receivedFrameCount.load(),
        exchange_.droppedEventCount.load(),
        exchange_.transmittedFrameCount.load(),
        exchange_.transmitFailureCount.load(),
        expectedNodeHbReplyMask_,
        receivedNodeHbReplyMask_,
        missedNodeHbReplyMask_,
        isNodeHbReplyWindowOpen_};
}

void cSupervisoryApplication::publishPendingFrame()
{
    while (true)
    {
        const std::optional<sCanFrame> frame = appStateMachine_.tryTakePendingCanFrame();
        if (!frame.has_value())
        {
            return;
        }

        if (!exchange_.transmitFrames.tryPush(*frame))
        {
            faultComms(ecCanCommsFaultReason::OutboundQueueFull);
            return;
        }
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
    if (const std::uint64_t progress = exchange_.commsProgress.load(); progress != lastCommsProgress_)
    {
        lastCommsProgress_ = progress;
        staleCommsProgressElapsed_ = std::chrono::milliseconds{0};
    }
    // Else if it is the same value the comms are hanging, add the elapsed ms.
    else if (exchange_.commsState.load() == ecCanCommsState::Running)
    {
        staleCommsProgressElapsed_ += elapsedMs;
    }

    // Check for standard comm failures
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
    while (exchange_.receivedNodeHbMessages.tryPop(message))
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
        frame.has_value() && !exchange_.transmitFrames.tryPush(*frame))
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
    static_cast<void>(exchange_.faultReason.compare_exchange_strong(expected, reason));
    isControlFaultLatched_ = true;

    sSupervisoryEvent event{};
    event.type = ecEventType::Fault;
    processControlEvent(event);
}

void cSupervisoryApplication::faultComms(const ecCanCommsFaultReason reason)
{
    if (isControlFaultLatched_)
    {
        return;
    }

    ecCanCommsFaultReason expected = ecCanCommsFaultReason::None;
    static_cast<void>(exchange_.faultReason.compare_exchange_strong(expected, reason));
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
    processControlEvent(event);
}

}
