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

#include "supervisory/common/event.hpp"
#include "supervisory/control/supervisory_state_machine.hpp"
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
    using project6::supervisory::ecSupervisoryControlState;
    using project6::supervisory::cSupervisoryStateMachineAPI;

    cSupervisoryStateMachineAPI stateMachine;

    stateMachine.handleEvent(makeFloorRequest(3));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::Dispatching,
        "floor request did not enter Dispatching");

    stateMachine.handleEvent(makeTimerTick(10ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::MovingUp,
        "target above current floor did not enter MovingUp");

    const std::optional<project6::supervisory::sCanFrame> firstCommand =
        stateMachine.tryTakePendingCanFrame();
    require(firstCommand.has_value(), "movement did not produce a CAN command");
    require(firstCommand->id == 0x100, "movement command used the wrong CAN identifier");
    require(firstCommand->dataLength == 1, "movement command used the wrong DLC");
    require(firstCommand->data[0] == 0x07, "movement command used the wrong target floor");
    require(
        !stateMachine.tryTakePendingCanFrame().has_value(),
        "movement command was returned more than once");

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

    const std::optional<project6::supervisory::sCanFrame> servicedFloorFrame =
        stateMachine.tryTakePendingCanFrame();
    require(servicedFloorFrame.has_value(), "arrival did not clear the serviced hall light");
    require(servicedFloorFrame->data[0] == 0x83, "arrival cleared the wrong hall light");

    const std::optional<project6::supervisory::sCanFrame> doorOpenFrame =
        stateMachine.tryTakePendingCanFrame();
    require(doorOpenFrame.has_value(), "arrival did not request the doors to open");
    require(doorOpenFrame->data[0] == 0x88, "arrival used the wrong door-open command");

    require(
        !stateMachine.tryTakePendingCanFrame().has_value(),
        "arrival produced an unexpected extra CAN frame");

    stateMachine.handleEvent(makeTimerTick(3000ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::Idle,
        "door timer did not return the state machine to Idle");

    const std::optional<project6::supervisory::sCanFrame> doorCloseFrame =
        stateMachine.tryTakePendingCanFrame();
    require(doorCloseFrame.has_value(), "arrival exit did not request the doors to close");
    require(doorCloseFrame->data[0] == 0x89, "arrival exit used the wrong door-close command");

    stateMachine.handleEvent(makeTimerTick(1ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::Dispatching,
        "queued request was not selected after arrival");

    stateMachine.handleEvent(makeTimerTick(1ms));
    require(
        stateMachine.snapshot().controlState == ecSupervisoryControlState::MovingDown,
        "queued lower-floor request did not enter MovingDown");

    const std::optional<project6::supervisory::sCanFrame> secondCommand =
        stateMachine.tryTakePendingCanFrame();
    require(secondCommand.has_value(), "queued request did not produce a second command");
    require(secondCommand->data[0] == 0x06, "queued request commanded the wrong floor");

    return 0;
}
