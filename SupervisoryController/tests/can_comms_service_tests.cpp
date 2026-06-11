#include <cstdlib>
#include <iostream>

#include "project6/supervisory/can/can_comms_service.hpp"

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

int main()
{
    using namespace project6::supervisory;

    sCanExchange exchange;
    require(
        exchange.commsState.load() == ecCanCommsState::Stopped,
        "COMMS state did not start stopped");
    require(
        exchange.faultReason.load() == ecCanCommsFaultReason::None,
        "COMMS exchange started with a fault");

    sSupervisoryEvent event{};
    event.type = ecEventType::CanFloorRequest;
    event.requestedFloor = 2;
    require(exchange.receivedEvents.tryPush(event), "inbound event was rejected");

    sSupervisoryEvent receivedEvent{};
    require(exchange.receivedEvents.tryPop(receivedEvent), "inbound event was lost");
    require(receivedEvent.requestedFloor == 2, "inbound event changed in transit");

    sCanFrame frame{};
    frame.id = kSupervisoryControllerCanId;
    frame.dataLength = 1;
    frame.data[0] = 0x07;
    require(exchange.transmitFrames.tryPush(frame), "outbound frame was rejected");

    sCanFrame receivedFrame{};
    require(exchange.transmitFrames.tryPop(receivedFrame), "outbound frame was lost");
    require(receivedFrame.data[0] == 0x07, "outbound frame changed in transit");

    const sSocketCanConfig config{"test", 125000, 100, false};
    cCanCommsService service(config, exchange);
    require(
        service.start() == ecOperationStatus::NotInitialized,
        "COMMS service started before adapter initialization");
    require(
        exchange.commsState.load() == ecCanCommsState::Stopped,
        "failed start changed COMMS state");

    return 0;
}
