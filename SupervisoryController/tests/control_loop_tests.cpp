#include <chrono>
#include <cstdlib>
#include <iostream>

#include "supervisory/app/supervisory_application.hpp"
#include "supervisory/can/can_comms_service.hpp"
#include "supervisory/drivers/supervisory_drivers.hpp"

namespace
{

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "TEST_FAILURE " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void markCommsProgress(project6::supervisory::sCanExchange& exchange)
{
    exchange.commsProgress.fetch_add(1);
}

} // namespace

namespace project6::supervisory::drivers
{

ecOperationStatus commandDoorOpen()
{
    return ecOperationStatus::Ok;
}

ecOperationStatus commandDoorClose()
{
    return ecOperationStatus::Ok;
}

ecOperationStatus commandEmergencyStop()
{
    return ecOperationStatus::Ok;
}

} // namespace project6::supervisory::drivers

int main()
{
    using namespace std::chrono_literals;
    using namespace project6::supervisory;

    sCanExchange exchange;
    sDBMessageExchange databaseExchange;
    exchange.commsState.store(ecCanCommsState::Running);
    exchange.commsProgress.store(1);

    cSupervisoryApplication application(exchange, databaseExchange);

    sSupervisoryEvent request{};
    request.type = ecEventType::CanFloorRequest;
    request.requestedFloor = 3;
    require(exchange.receivedEvents.tryPush(request), "request setup failed");
    require(
        application.runControlCycle(10ms) == ecOperationStatus::Ok,
        "control cycle rejected a valid request");
    require(
        application.snapshot().controlState == ecSupervisoryControlState::MovingUp,
        "control cycle did not advance the request");

    sCanFrame command{};
    require(exchange.transmitFrames.tryPop(command), "control cycle did not publish a command");
    require(command.data[0] == 0x07, "control cycle published the wrong command");

    sSupervisoryEvent arrival{};
    arrival.type = ecEventType::CanElevatorStatus;
    arrival.reportedFloor = 3;
    require(exchange.receivedEvents.tryPush(arrival), "arrival setup failed");
    require(
        application.runControlCycle(10ms) == ecOperationStatus::Ok,
        "control cycle rejected an elevator arrival");
    require(exchange.transmitFrames.tryPop(command), "arrival did not publish a hall-light command");
    require(command.data[0] == 0x83, "arrival published the wrong hall-light command");
    require(exchange.transmitFrames.tryPop(command), "arrival did not publish a door-open command");
    require(command.data[0] == 0x88, "arrival published the wrong door-open command");

    markCommsProgress(exchange);
    require(
        application.runControlCycle(3s) == ecOperationStatus::Ok,
        "control cycle rejected the door dwell timeout");
    require(exchange.transmitFrames.tryPop(command), "arrival exit did not publish a door-close command");
    require(command.data[0] == 0x89, "arrival exit published the wrong door-close command");

    require(
        application.runControlCycle(249ms) == ecOperationStatus::Ok,
        "control cycle stopped while COMMS progress was stale");
    require(
        application.snapshot().controlState != ecSupervisoryControlState::Faulted,
        "COMMS froze before the timeout");

    require(
        application.runControlCycle(1ms) == ecOperationStatus::Ok,
        "control cycle stopped after COMMS timeout");
    require(
        application.snapshot().controlState == ecSupervisoryControlState::Faulted,
        "stale COMMS progress did not fault the state machine");
    require(
        application.canHealth().faultReason == ecCanCommsFaultReason::CommsProgressTimeout,
        "operator health did not identify the frozen COMMS thread");

    sCanExchange heartbeatExchange;
    sDBMessageExchange heartbeatDatabaseExchange;
    heartbeatExchange.commsState.store(ecCanCommsState::Stopped);
    heartbeatExchange.commsProgress.store(1);
    cSupervisoryApplication heartbeatApplication(heartbeatExchange, heartbeatDatabaseExchange);

    markCommsProgress(heartbeatExchange);
    require(
        heartbeatApplication.runControlCycle(3999ms) == ecOperationStatus::Ok,
        "node heartbeat warmup cycle failed");
    require(
        !heartbeatExchange.transmitFrames.tryPop(command),
        "node heartbeat request was sent before the interval elapsed");

    markCommsProgress(heartbeatExchange);
    require(
        heartbeatApplication.runControlCycle(1ms) == ecOperationStatus::Ok,
        "node heartbeat request cycle failed");
    require(
        heartbeatExchange.transmitFrames.tryPop(command),
        "node heartbeat request was not published after the interval elapsed");
    require(command.id == kSupervisoryControllerCanId, "node heartbeat request used the wrong ID");
    require(command.dataLength == 1, "node heartbeat request used the wrong DLC");
    require(command.data[0] == 0x85, "node heartbeat request used the wrong payload");

    require(
        heartbeatApplication.canHealth().isNodeHbReplyWindowOpen,
        "node heartbeat reply window did not open");
    require(
        heartbeatApplication.canHealth().expectedNodeHbReplyMask == kExpectedNodeHbReplyMask,
        "node heartbeat expected mask was not exposed");

    require(heartbeatExchange.receivedNodeHbMessages.tryPush(
        sNodeHbMessage{ecNodeHb::NodeRequest, kFloorOneControllerCanId, 0x86}),
        "node heartbeat request setup failed");
    markCommsProgress(heartbeatExchange);
    require(
        heartbeatApplication.runControlCycle(10ms) == ecOperationStatus::Ok,
        "node heartbeat reply cycle failed");
    require(
        heartbeatExchange.transmitFrames.tryPop(command),
        "supervisor did not reply to node heartbeat request");
    require(command.id == kSupervisoryControllerCanId, "node heartbeat reply used the wrong ID");
    require(command.data[0] == 0x84, "node heartbeat reply used the wrong payload");

    sCanExchange allReplyExchange;
    sDBMessageExchange allReplyDatabaseExchange;
    allReplyExchange.commsState.store(ecCanCommsState::Stopped);
    allReplyExchange.commsProgress.store(1);
    cSupervisoryApplication allReplyApplication(allReplyExchange, allReplyDatabaseExchange);

    markCommsProgress(allReplyExchange);
    require(allReplyApplication.runControlCycle(4s) == ecOperationStatus::Ok,
        "node heartbeat request setup cycle failed");
    require(allReplyExchange.transmitFrames.tryPop(command), "node heartbeat request setup missing");

    require(allReplyExchange.receivedNodeHbMessages.tryPush(
        sNodeHbMessage{ecNodeHb::Ok, kCarControllerCanId, 0x84}),
        "car controller heartbeat reply setup failed");
    require(allReplyExchange.receivedNodeHbMessages.tryPush(
        sNodeHbMessage{ecNodeHb::Ok, kFloorOneControllerCanId, 0x84}),
        "floor 1 heartbeat reply setup failed");
    require(allReplyExchange.receivedNodeHbMessages.tryPush(
        sNodeHbMessage{ecNodeHb::Ok, kFloorTwoControllerCanId, 0x84}),
        "floor 2 heartbeat reply setup failed");
    require(allReplyExchange.receivedNodeHbMessages.tryPush(
        sNodeHbMessage{ecNodeHb::Ok, kFloorThreeControllerCanId, 0x84}),
        "floor 3 heartbeat reply setup failed");

    markCommsProgress(allReplyExchange);
    require(allReplyApplication.runControlCycle(1000ms) == ecOperationStatus::Ok,
        "all heartbeat replies cycle failed");
    require(
        !allReplyApplication.canHealth().isNodeHbReplyWindowOpen,
        "complete node heartbeat reply set did not close the window");
    require(
        allReplyApplication.canHealth().receivedNodeHbReplyMask == kExpectedNodeHbReplyMask,
        "complete node heartbeat reply set did not update the mask");
    require(
        allReplyApplication.snapshot().controlState != ecSupervisoryControlState::Faulted,
        "complete node heartbeat reply set faulted the state machine");

    sCanExchange missedReplyExchange;
    sDBMessageExchange missedReplyDatabaseExchange;
    missedReplyExchange.commsState.store(ecCanCommsState::Stopped);
    missedReplyExchange.commsProgress.store(1);
    cSupervisoryApplication missedReplyApplication(missedReplyExchange, missedReplyDatabaseExchange);

    markCommsProgress(missedReplyExchange);
    require(missedReplyApplication.runControlCycle(4s) == ecOperationStatus::Ok,
        "missed heartbeat request setup cycle failed");
    require(missedReplyExchange.transmitFrames.tryPop(command), "missed heartbeat request missing");
    require(missedReplyExchange.receivedNodeHbMessages.tryPush(
        sNodeHbMessage{ecNodeHb::Ok, kCarControllerCanId, 0x84}),
        "partial heartbeat reply setup failed");

    markCommsProgress(missedReplyExchange);
    require(missedReplyApplication.runControlCycle(2999ms) == ecOperationStatus::Ok,
        "node heartbeat verification window closed early");
    require(
        missedReplyApplication.snapshot().controlState != ecSupervisoryControlState::Faulted,
        "node heartbeat faulted before the verification window elapsed");

    markCommsProgress(missedReplyExchange);
    require(missedReplyApplication.runControlCycle(1ms) == ecOperationStatus::Ok,
        "node heartbeat timeout cycle failed");
    require(
        missedReplyApplication.snapshot().controlState == ecSupervisoryControlState::Faulted,
        "missing node heartbeat replies did not fault control");
    require(
        missedReplyApplication.canHealth().faultReason == ecCanCommsFaultReason::NodeHeartbeatTimeout,
        "node heartbeat timeout was not reported");
    require(
        missedReplyApplication.canHealth().missedNodeHbReplyMask ==
            static_cast<std::uint8_t>(kExpectedNodeHbReplyMask & ~kNodeHbCcMask),
        "missed node heartbeat mask did not identify missing nodes");

    sCanExchange nodeErrorExchange;
    sDBMessageExchange nodeErrorDatabaseExchange;
    nodeErrorExchange.commsState.store(ecCanCommsState::Stopped);
    nodeErrorExchange.commsProgress.store(1);
    cSupervisoryApplication nodeErrorApplication(nodeErrorExchange, nodeErrorDatabaseExchange);

    require(nodeErrorExchange.receivedNodeHbMessages.tryPush(
        sNodeHbMessage{ecNodeHb::Error, kFloorTwoControllerCanId, 0x87}),
        "node heartbeat error setup failed");
    markCommsProgress(nodeErrorExchange);
    require(nodeErrorApplication.runControlCycle(10ms) == ecOperationStatus::Ok,
        "node heartbeat error cycle failed");
    require(
        nodeErrorApplication.snapshot().controlState == ecSupervisoryControlState::Faulted,
        "node heartbeat error did not fault control");
    require(
        nodeErrorApplication.canHealth().faultReason == ecCanCommsFaultReason::NodeHeartbeatError,
        "node heartbeat error was not reported");
    require(
        nodeErrorApplication.canHealth().missedNodeHbReplyMask == kNodeHbFc2Mask,
        "node heartbeat error did not identify the source node");

    sCanExchange logOnlyExchange;
    sDBMessageExchange logOnlyDatabaseExchange;
    logOnlyExchange.commsState.store(ecCanCommsState::Stopped);
    logOnlyExchange.commsProgress.store(1);
    cSupervisoryApplication logOnlyApplication(
        logOnlyExchange, logOnlyDatabaseExchange, ecNodeHbFailureMode::LogOnly);

    markCommsProgress(logOnlyExchange);
    require(logOnlyApplication.runControlCycle(4s) == ecOperationStatus::Ok,
        "log-only heartbeat request setup failed");
    require(logOnlyExchange.transmitFrames.tryPop(command), "log-only heartbeat request missing");
    markCommsProgress(logOnlyExchange);
    require(logOnlyApplication.runControlCycle(3s) == ecOperationStatus::Ok,
        "log-only heartbeat timeout cycle failed");
    require(
        logOnlyApplication.snapshot().controlState != ecSupervisoryControlState::Faulted,
        "log-only node heartbeat timeout faulted control");
    require(
        logOnlyApplication.canHealth().missedNodeHbReplyMask == kExpectedNodeHbReplyMask,
        "log-only node heartbeat timeout was not reported");

    return 0;
}
