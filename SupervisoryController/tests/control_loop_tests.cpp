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
    exchange.commsState.store(ecCanCommsState::Running);
    exchange.heartbeat.store(1);

    cSupervisoryApplication application(exchange);

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

    require(
        application.runControlCycle(249ms) == ecOperationStatus::Ok,
        "control cycle stopped while COMMS heartbeat was stale");
    require(
        application.snapshot().controlState != ecSupervisoryControlState::Faulted,
        "COMMS froze before the timeout");

    require(
        application.runControlCycle(1ms) == ecOperationStatus::Ok,
        "control cycle stopped after COMMS timeout");
    require(
        application.snapshot().controlState == ecSupervisoryControlState::Faulted,
        "stale COMMS heartbeat did not fault the state machine");
    require(
        application.canHealth().faultReason == ecCanCommsFaultReason::HeartbeatTimeout,
        "operator health did not identify the frozen COMMS thread");

    return 0;
}
