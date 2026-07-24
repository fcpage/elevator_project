/******************************************************************
* modes_audio_tests.cpp - Phase 2 maintenance/Sabbath/audio tests
******************************************************************/

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "supervisory/audio/announcement_service.hpp"
#include "supervisory/app/supervisory_application.hpp"
#include "supervisory/can/can_comms_service.hpp"
#include "supervisory/control/supervisory_state_machine.hpp"
#include "supervisory/scheduler/request_scheduler.hpp"

namespace
{

using namespace project6::supervisory;

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "TEST_FAILURE " << message << '\n';
        std::exit(1);
    }
}

sSupervisoryEvent request(const ecEventType type, const std::uint8_t floor)
{
    sSupervisoryEvent event{};
    event.type = type;
    event.requestedFloor = floor;
    return event;
}

sSupervisoryEvent mode(const std::uint8_t bits)
{
    sSupervisoryEvent event{};
    event.type = ecEventType::ModeUpdate;
    event.modeBits = bits;
    return event;
}

sSupervisoryEvent tick(const std::chrono::milliseconds elapsed)
{
    sSupervisoryEvent event{};
    event.type = ecEventType::TimerTick;
    event.timestampMs = elapsed;
    return event;
}

void testSchedulerPriorityAndGates()
{
    cRequestScheduler scheduler;
    scheduler.enqueueEvent(request(ecEventType::CanCarRequest, 1));
    scheduler.enqueueEvent(request(ecEventType::CanFloorRequest, 2));
    scheduler.enqueueEvent(request(ecEventType::HttpFloorRequest, 3));
    scheduler.enqueueEvent(request(ecEventType::MaintenanceFloorRequest, 2));
    scheduler.enqueueSabbathRequest(1);

    require(
        scheduler.tryTakeNextAllowedRequest(kModeMaintenance)->source == ecRequestSource::Maintenance,
        "maintenance mode did not gate to maintenance queue");
    require(
        scheduler.tryTakeNextAllowedRequest(kModeSabbath)->source == ecRequestSource::Sabbath,
        "Sabbath mode did not gate to Sabbath queue");

    const auto normal = scheduler.tryTakeNextAllowedRequest(0);
    require(normal.has_value() && normal->source == ecRequestSource::CarModule,
        "normal mode did not preserve car priority");
}

void testStateMachineModes()
{
    cSupervisoryStateMachineAPI maintenanceMachine;
    maintenanceMachine.handleEvent(mode(kModeMaintenance));
    maintenanceMachine.handleEvent(request(ecEventType::CanCarRequest, 3));
    maintenanceMachine.handleEvent(tick(std::chrono::milliseconds{1}));
    require(
        maintenanceMachine.snapshot().controlState == ecSupervisoryControlState::Idle,
        "maintenance mode served a normal request");

    maintenanceMachine.handleEvent(request(ecEventType::MaintenanceFloorRequest, 2));
    maintenanceMachine.handleEvent(tick(std::chrono::milliseconds{1}));
    require(
        maintenanceMachine.snapshot().controlState != ecSupervisoryControlState::Idle,
        "maintenance request was not dispatched");

    cSupervisoryStateMachineAPI sabbathMachine;
    sabbathMachine.setSabbathStopDuration(std::chrono::milliseconds{1000});
    sabbathMachine.handleEvent(mode(kModeSabbath));
    sabbathMachine.handleEvent(request(ecEventType::CanCarRequest, 3));
    sabbathMachine.handleEvent(tick(std::chrono::milliseconds{1000}));
    require(
        sabbathMachine.snapshot().modeBits == kModeSabbath,
        "Sabbath mode bit was not applied");
    require(
        sabbathMachine.snapshot().controlState != ecSupervisoryControlState::Idle,
        "Sabbath mode did not generate an automatic request");
}

void testAnnouncementServiceDemoSink()
{
    sAnnouncementExchange exchange;
    const sAnnouncementServiceConfig config{true, "audio", "Headphones", 1.0F};
    cAnnouncementService service(config, exchange);
    require(service.start(), "announcement demo service did not start");
    require(service.submit(2), "announcement request was rejected");

    for (int attempt = 0; attempt < 50 && exchange.played.load() == 0; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    service.stop();
    require(exchange.played.load() == 1, "announcement demo sink did not consume request");
}

void testArrivalTriggersOneAnnouncement()
{
    sCanExchange canExchange;
    sAnnouncementExchange audioExchange;
    const sAnnouncementServiceConfig config{true, "audio", "Headphones", 1.0F};
    cAnnouncementService service(config, audioExchange);
    require(service.start(), "arrival announcement service did not start");

    cSupervisoryApplication application(
        canExchange,
        ecNodeHbFailureMode::FaultControl,
        &service);

    require(canExchange.receivedEvents.tryPush(
        request(ecEventType::CanFloorRequest, 2)),
        "arrival request setup failed");
    application.runControlCycle(std::chrono::milliseconds{1});

    sSupervisoryEvent arrival{};
    arrival.type = ecEventType::CanElevatorStatus;
    arrival.reportedFloor = 2;
    require(canExchange.receivedEvents.tryPush(arrival), "arrival status setup failed");
    application.runControlCycle(std::chrono::milliseconds{1});
    application.runControlCycle(std::chrono::milliseconds{1});

    for (int attempt = 0; attempt < 50 && audioExchange.played.load() == 0; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    require(audioExchange.played.load() == 1, "arrival did not produce an announcement");

    require(canExchange.receivedEvents.tryPush(arrival), "duplicate arrival setup failed");
    application.runControlCycle(std::chrono::milliseconds{1});
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    require(audioExchange.played.load() == 1, "duplicate arrival produced an announcement");
    service.stop();
}

} // namespace

int main()
{
    testSchedulerPriorityAndGates();
    testStateMachineModes();
    testAnnouncementServiceDemoSink();
    testArrivalTriggersOneAnnouncement();
    std::cout << "MODES_AUDIO_TESTS_PASSED\n";
    return 0;
}
