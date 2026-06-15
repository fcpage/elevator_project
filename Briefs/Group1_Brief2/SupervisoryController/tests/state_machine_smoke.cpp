/******************************************************************
* state_machine_smoke.cpp - Supervisory State Machine Smoke Test
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Verifies command dispatch, auto-arrival, and queued requests.
******************************************************************/

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <optional>

#include "project6/supervisory/can/can_adapter.hpp"
#include "project6/supervisory/common/event.hpp"
#include "project6/supervisory/control/supervisory_state_machine.hpp"
#include "project6/supervisory/drivers/supervisory_drivers.hpp"

namespace
{

std::optional<std::uint8_t> lastCommandedFloor;
std::size_t commandCount = 0;

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "TEST_FAILURE " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

project6::supervisory::sSupervisoryEvent makeFloorRequest(const std::uint8_t floor)
{
    project6::supervisory::sSupervisoryEvent event{};
    event.type = project6::supervisory::ecEventType::CanFloorRequest;
    event.requestedFloor = floor;
    return event;
}

project6::supervisory::sSupervisoryEvent makeTimerTick(
    const std::chrono::milliseconds elapsedMs)
{
    project6::supervisory::sSupervisoryEvent event{};
    event.type = project6::supervisory::ecEventType::TimerTick;
    event.timestampMs = elapsedMs;
    return event;
}

#ifndef SUPERVISORY_TEST_EXPECT_AUTO_ARRIVAL
project6::supervisory::sSupervisoryEvent makeElevatorStatus(const std::uint8_t floor)
{
    project6::supervisory::sSupervisoryEvent event{};
    event.type = project6::supervisory::ecEventType::CanElevatorStatus;
    event.reportedFloor = floor;
    return event;
}
#endif

} // namespace

namespace project6::supervisory::drivers
{

ecOperationStatus commandElevatorToFloor(
    cSocketCanAdapter& canAdapter,
    const std::uint8_t targetFloor)
{
    static_cast<void>(canAdapter);
    lastCommandedFloor = targetFloor;
    ++commandCount;
    return ecOperationStatus::Ok;
}

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
    using project6::supervisory::cSocketCanAdapter;
    using project6::supervisory::ecSupervisoryControlState;
    using project6::supervisory::cSupervisoryStateMachineAPI;

    cSocketCanAdapter canAdapter("test");
    cSupervisoryStateMachineAPI stateMachine(canAdapter);

    stateMachine.handleEvent(makeFloorRequest(3));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::Dispatching,
        "floor request did not enter Dispatching");

    stateMachine.handleEvent(makeTimerTick(10ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::MovingUp,
        "target above current floor did not enter MovingUp");
    require(lastCommandedFloor == 3, "movement command used the wrong target floor");
    require(commandCount == 1, "movement command was not sent exactly once");

    stateMachine.handleEvent(makeFloorRequest(2));
    stateMachine.handleEvent(makeTimerTick(2999ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::MovingUp,
        "auto-arrival occurred before three seconds");

    stateMachine.handleEvent(makeTimerTick(1ms));

#ifdef SUPERVISORY_TEST_EXPECT_AUTO_ARRIVAL
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::Arrived,
        "auto-arrival did not occur at three seconds");
#else
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::MovingUp,
        "production mode arrived without elevator status");

    stateMachine.handleEvent(makeElevatorStatus(3));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::Arrived,
        "matching elevator status did not record arrival");
#endif

    require(stateMachine.snapshot().currentFloor == 3, "arrival recorded the wrong floor");

    stateMachine.handleEvent(makeTimerTick(3000ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::Idle,
        "door timer did not return the state machine to Idle");

    stateMachine.handleEvent(makeTimerTick(1ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::Dispatching,
        "queued request was not selected after arrival");

    stateMachine.handleEvent(makeTimerTick(1ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::MovingDown,
        "queued lower-floor request did not enter MovingDown");
    require(lastCommandedFloor == 2, "queued request commanded the wrong floor");
    require(commandCount == 2, "queued request did not produce a second command");

    return 0;
}
