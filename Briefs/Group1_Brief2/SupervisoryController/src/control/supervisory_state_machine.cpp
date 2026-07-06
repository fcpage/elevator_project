/******************************************************************
* supervisory_state_machine.cpp - ESE Supervisory State Machine
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Holds the ESE-readable state-machine shape and public API.
******************************************************************/

#include "project6/supervisory/control/supervisory_state_machine.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>

#include "project6/supervisory/can/can_adapter.hpp"
#include "project6/supervisory/drivers/supervisory_drivers.hpp"
#include "project6/supervisory/scheduler/request_scheduler.hpp"

namespace project6::supervisory
{

struct sStateMachineContext
{
    explicit sStateMachineContext(cSocketCanAdapter& adapter)
        : canAdapter(adapter)
    {
    }

    cSocketCanAdapter& canAdapter;
    cRequestScheduler scheduler;
    std::optional<sElevatorRequest> activeRequest;
    sSupervisoryStateSnapshot snapshot{};
    std::chrono::milliseconds doorOpenElapsedMs{0};
    std::chrono::milliseconds movementElapsedMs{0};
    bool didElevatorReportArrival = false;
};

namespace
{

constexpr std::chrono::milliseconds kDoorOpenDurationMs{3000};
#ifdef SUPERVISORY_ENABLE_AUTO_ARRIVAL
constexpr std::chrono::seconds kSimulatedTravelDuration{3};
#endif
sStateMachineContext* activeContext = nullptr;

sStateMachineContext* context()
{
    return activeContext;
}

bool hasPendingRequest()
{
    sStateMachineContext* state = context();
    if (state == nullptr)
    {
        return false;
    }

    return state->activeRequest.has_value() || state->scheduler.hasPendingRequest();
}

bool targetIsCurrentFloor()
{
    sStateMachineContext* state = context();
    if (state == nullptr || !state->activeRequest.has_value())
    {
        return false;
    }

    return state->activeRequest->floor == state->snapshot.currentFloor;
}

bool targetAboveCurrentFloor()
{
    sStateMachineContext* state = context();
    if (state == nullptr || !state->activeRequest.has_value())
    {
        return false;
    }

    return state->activeRequest->floor > state->snapshot.currentFloor;
}

bool targetBelowCurrentFloor()
{
    sStateMachineContext* state = context();
    if (state == nullptr || !state->activeRequest.has_value())
    {
        return false;
    }

    return state->activeRequest->floor < state->snapshot.currentFloor;
}

bool elevatorReportedArrival()
{
    sStateMachineContext* state = context();
    if (state == nullptr)
    {
        return false;
    }

    return state->didElevatorReportArrival;
}

bool doorWaitExpired()
{
    sStateMachineContext* state = context();
    if (state == nullptr)
    {
        return false;
    }

    return state->doorOpenElapsedMs >= kDoorOpenDurationMs;
}

bool faultDetected()
{
    sStateMachineContext* state = context();
    if (state == nullptr)
    {
        return false;
    }

    return state->snapshot.isFaulted;
}

void selectNextRequest()
{
    sStateMachineContext* state = context();
    if (state == nullptr || state->activeRequest.has_value())
    {
        return;
    }

    state->activeRequest = state->scheduler.tryTakeNextRequest();
    if (state->activeRequest.has_value())
    {
        state->snapshot.targetFloor = state->activeRequest->floor;
    }
}

void commandElevatorToTarget()
{
    sStateMachineContext* state = context();
    if (state == nullptr || !state->activeRequest.has_value())
    {
        return;
    }

    if (state->activeRequest->floor > state->snapshot.currentFloor)
    {
        state->snapshot.direction = ecTravelDirection::Up;
    }
    else if (state->activeRequest->floor < state->snapshot.currentFloor)
    {
        state->snapshot.direction = ecTravelDirection::Down;
    }
    else
    {
        state->snapshot.direction = ecTravelDirection::None;
    }

    state->movementElapsedMs = std::chrono::milliseconds{0};
    state->didElevatorReportArrival = false;

    const ecOperationStatus status =
        drivers::commandElevatorToFloor(state->canAdapter, state->activeRequest->floor);
    if (status != ecOperationStatus::Ok)
    {
        state->snapshot.isFaulted = true;
    }
}

void recordArrival()
{
    sStateMachineContext* state = context();
    if (state == nullptr)
    {
        return;
    }

    if (state->activeRequest.has_value())
    {
        state->snapshot.currentFloor = state->activeRequest->floor;
        state->snapshot.targetFloor = state->activeRequest->floor;
    }

    state->snapshot.direction = ecTravelDirection::None;
    state->snapshot.isDoorOpen = true;
    state->doorOpenElapsedMs = std::chrono::milliseconds{0};
    state->didElevatorReportArrival = false;

    const ecOperationStatus status = drivers::commandDoorOpen();
    if (status != ecOperationStatus::Ok)
    {
        state->snapshot.isFaulted = true;
    }
}

void clearServicedRequest()
{
    sStateMachineContext* state = context();
    if (state == nullptr)
    {
        return;
    }

    state->activeRequest.reset();
    state->snapshot.direction = ecTravelDirection::None;
    state->snapshot.isDoorOpen = false;
    state->doorOpenElapsedMs = std::chrono::milliseconds{0};
    state->movementElapsedMs = std::chrono::milliseconds{0};

    const ecOperationStatus status = drivers::commandDoorClose();
    if (status != ecOperationStatus::Ok)
    {
        state->snapshot.isFaulted = true;
    }
}

void enterFaultState()
{
    sStateMachineContext* state = context();
    if (state == nullptr)
    {
        return;
    }

    state->snapshot.direction = ecTravelDirection::None;
    state->snapshot.isFaulted = true;
    static_cast<void>(drivers::commandEmergencyStop());
}

} // namespace

/* #ESE-BEGIN

machine SupervisoryController {
    initial Idle;

    state Idle {
        on faultDetected -> Faulted;
        on pendingRequestAvailable -> Dispatching;
    }

    state Dispatching {
        on faultDetected -> Faulted;
        on targetIsCurrentFloor -> Arrived;
        on targetAboveCurrentFloor -> MovingUp;
        on targetBelowCurrentFloor -> MovingDown;
    }

    state MovingUp {
        on faultDetected -> Faulted;
        on elevatorReportedArrival -> Arrived;
    }

    state MovingDown {
        on faultDetected -> Faulted;
        on elevatorReportedArrival -> Arrived;
    }

    state Arrived {
        on faultDetected -> Faulted;
        on doorWaitExpired -> Idle;
    }

    state Faulted {
    }
}

conditionals SupervisoryController {
    pendingRequestAvailable = hasPendingRequest();
    targetIsCurrentFloor = targetIsCurrentFloor();
    targetAboveCurrentFloor = targetAboveCurrentFloor();
    targetBelowCurrentFloor = targetBelowCurrentFloor();
    elevatorReportedArrival = elevatorReportedArrival();
    doorWaitExpired = doorWaitExpired();
    faultDetected = faultDetected();
}

actions SupervisoryController {
    enter Dispatching {
        selectNextRequest();
    }

    enter MovingUp {
        commandElevatorToTarget();
    }

    enter MovingDown {
        commandElevatorToTarget();
    }

    enter Arrived {
        recordArrival();
    }

    exit Arrived {
        clearServicedRequest();
    }

    enter Faulted {
        enterFaultState();
    }
}

#ESE-END */

// #ESE-GENERATED-BEGIN: SupervisoryController
// Generated by esepp. Do not edit this section by hand.

class cSupervisoryController
{
public:
    enum class State
    {
        Idle,
        Dispatching,
        MovingUp,
        MovingDown,
        Arrived,
        Faulted
    };

    cSupervisoryController()
    {
        enterIdle();
    }

    void update()
    {
        switch (currentState)
        {
            case State::Idle:
            {
                updateIdle();
                break;
            }

            case State::Dispatching:
            {
                updateDispatching();
                break;
            }

            case State::MovingUp:
            {
                updateMovingUp();
                break;
            }

            case State::MovingDown:
            {
                updateMovingDown();
                break;
            }

            case State::Arrived:
            {
                updateArrived();
                break;
            }

            case State::Faulted:
            {
                updateFaulted();
                break;
            }

        }
    }

    State getState() const
    {
        return currentState;
    }

private:
    State currentState = State::Idle;

    void updateIdle()
    {
        if (conditionFaultDetected())
        {
            transitionTo(State::Faulted);
            return;
        }
        if (conditionPendingRequestAvailable())
        {
            transitionTo(State::Dispatching);
            return;
        }
    }

    void updateDispatching()
    {
        if (conditionFaultDetected())
        {
            transitionTo(State::Faulted);
            return;
        }
        if (conditionTargetIsCurrentFloor())
        {
            transitionTo(State::Arrived);
            return;
        }
        if (conditionTargetAboveCurrentFloor())
        {
            transitionTo(State::MovingUp);
            return;
        }
        if (conditionTargetBelowCurrentFloor())
        {
            transitionTo(State::MovingDown);
            return;
        }
    }

    void updateMovingUp()
    {
        if (conditionFaultDetected())
        {
            transitionTo(State::Faulted);
            return;
        }
        if (conditionElevatorReportedArrival())
        {
            transitionTo(State::Arrived);
            return;
        }
    }

    void updateMovingDown()
    {
        if (conditionFaultDetected())
        {
            transitionTo(State::Faulted);
            return;
        }
        if (conditionElevatorReportedArrival())
        {
            transitionTo(State::Arrived);
            return;
        }
    }

    void updateArrived()
    {
        if (conditionFaultDetected())
        {
            transitionTo(State::Faulted);
            return;
        }
        if (conditionDoorWaitExpired())
        {
            transitionTo(State::Idle);
            return;
        }
    }

    void updateFaulted()
    {
    }

    bool conditionPendingRequestAvailable()
    {
        return hasPendingRequest();
    }

    bool conditionTargetIsCurrentFloor()
    {
        return targetIsCurrentFloor();
    }

    bool conditionTargetAboveCurrentFloor()
    {
        return targetAboveCurrentFloor();
    }

    bool conditionTargetBelowCurrentFloor()
    {
        return targetBelowCurrentFloor();
    }

    bool conditionElevatorReportedArrival()
    {
        return elevatorReportedArrival();
    }

    bool conditionDoorWaitExpired()
    {
        return doorWaitExpired();
    }

    bool conditionFaultDetected()
    {
        return faultDetected();
    }

    void transitionTo(State nextState)
    {
        if (currentState == nextState)
        {
            return;
        }

        switch (currentState)
        {
            case State::Idle:
            {
                exitIdle();
                break;
            }

            case State::Dispatching:
            {
                exitDispatching();
                break;
            }

            case State::MovingUp:
            {
                exitMovingUp();
                break;
            }

            case State::MovingDown:
            {
                exitMovingDown();
                break;
            }

            case State::Arrived:
            {
                exitArrived();
                break;
            }

            case State::Faulted:
            {
                exitFaulted();
                break;
            }

        }

        currentState = nextState;

        switch (currentState)
        {
            case State::Idle:
            {
                enterIdle();
                break;
            }

            case State::Dispatching:
            {
                enterDispatching();
                break;
            }

            case State::MovingUp:
            {
                enterMovingUp();
                break;
            }

            case State::MovingDown:
            {
                enterMovingDown();
                break;
            }

            case State::Arrived:
            {
                enterArrived();
                break;
            }

            case State::Faulted:
            {
                enterFaulted();
                break;
            }

        }
    }

    void enterIdle()
    {
    }

    void enterDispatching()
    {
        selectNextRequest();
    }

    void enterMovingUp()
    {
        commandElevatorToTarget();
    }

    void enterMovingDown()
    {
        commandElevatorToTarget();
    }

    void enterArrived()
    {
        recordArrival();
    }

    void enterFaulted()
    {
        enterFaultState();
    }

    void exitIdle()
    {
    }

    void exitDispatching()
    {
    }

    void exitMovingUp()
    {
    }

    void exitMovingDown()
    {
    }

    void exitArrived()
    {
        clearServicedRequest();
    }

    void exitFaulted()
    {
    }

};

// #ESE-GENERATED-END: SupervisoryController

namespace
{

const char* controlStateName(const ecSupervisoryControlState state)
{
    switch (state)
    {
        case ecSupervisoryControlState::Idle:
            return "Idle";
        case ecSupervisoryControlState::Dispatching:
            return "Dispatching";
        case ecSupervisoryControlState::MovingUp:
            return "MovingUp";
        case ecSupervisoryControlState::MovingDown:
            return "MovingDown";
        case ecSupervisoryControlState::Arrived:
            return "Arrived";
        case ecSupervisoryControlState::Faulted:
            return "Faulted";
    }

    return "Unknown";
}

const char* directionName(const ecTravelDirection direction)
{
    switch (direction)
    {
        case ecTravelDirection::None:
            return "None";
        case ecTravelDirection::Up:
            return "Up";
        case ecTravelDirection::Down:
            return "Down";
    }

    return "Unknown";
}

bool snapshotsDiffer(
    const sSupervisoryStateSnapshot& previous,
    const sSupervisoryStateSnapshot& current)
{
    return previous.controlState != current.controlState ||
           previous.currentFloor != current.currentFloor ||
           previous.targetFloor != current.targetFloor ||
           previous.direction != current.direction ||
           previous.isDoorOpen != current.isDoorOpen ||
           previous.isFaulted != current.isFaulted;
}

void logStateChange(
    const sSupervisoryStateSnapshot& previous,
    const sSupervisoryStateSnapshot& current)
{
    if (!snapshotsDiffer(previous, current))
    {
        return;
    }

    std::clog << "STATE " << controlStateName(previous.controlState)
              << " -> " << controlStateName(current.controlState)
              << " current=" << static_cast<unsigned int>(current.currentFloor)
              << " target=" << static_cast<unsigned int>(current.targetFloor)
              << " direction=" << directionName(current.direction)
              << " door_open=" << (current.isDoorOpen ? "true" : "false")
              << " faulted=" << (current.isFaulted ? "true" : "false") << '\n';
}

bool isMoving(const ecSupervisoryControlState state)
{
    return state == ecSupervisoryControlState::MovingUp ||
           state == ecSupervisoryControlState::MovingDown;
}

} // namespace

cSupervisoryStateMachineAPI::cSupervisoryStateMachineAPI(cSocketCanAdapter& canAdapter)
    : smContext_(std::make_unique<sStateMachineContext>(canAdapter))
{
    activeContext = smContext_.get();
    smMachine_ = std::make_unique<cSupervisoryController>();
    refreshSnapshotState();
}

cSupervisoryStateMachineAPI::~cSupervisoryStateMachineAPI()
{
    if (activeContext == smContext_.get())
    {
        activeContext = nullptr;
    }
}

void cSupervisoryStateMachineAPI::handleEvent(const sSupervisoryEvent& event)
{
    sStateMachineContext& state = *smContext_;
    const sSupervisoryStateSnapshot previousSnapshot = state.snapshot;

    if (event.type == ecEventType::HttpFloorRequest ||
        event.type == ecEventType::CanFloorRequest ||
        event.type == ecEventType::CanCarRequest)
    {
        state.scheduler.enqueueEvent(event);
    }

    if (event.reportedFloor.has_value())
    {
        state.snapshot.currentFloor = *event.reportedFloor;
    }

    if (event.reportedDirection != ecTravelDirection::None)
    {
        state.snapshot.direction = event.reportedDirection;
    }

    if (event.type == ecEventType::CanElevatorStatus &&
        state.activeRequest.has_value() &&
        event.reportedFloor.has_value() &&
        *event.reportedFloor == state.activeRequest->floor)
    {
        state.didElevatorReportArrival = true;
    }

    if (event.type == ecEventType::TimerTick && state.snapshot.isDoorOpen)
    {
        state.doorOpenElapsedMs += event.timestampMs;
    }

    if (event.type == ecEventType::TimerTick && isMoving(state.snapshot.controlState))
    {
        state.movementElapsedMs += event.timestampMs;

#ifdef SUPERVISORY_ENABLE_AUTO_ARRIVAL
        if (!state.didElevatorReportArrival &&
            state.movementElapsedMs >= kSimulatedTravelDuration)
        {
            state.didElevatorReportArrival = true;
            std::clog << "AUTO_ARRIVAL target_floor="
                      << static_cast<unsigned int>(state.snapshot.targetFloor)
                      << " elapsed_ms=" << state.movementElapsedMs.count() << '\n';
        }
#endif
    }

    if (event.type == ecEventType::Fault)
    {
        state.snapshot.isFaulted = true;
    }

    smMachine_->update();
    refreshSnapshotState();
    logStateChange(previousSnapshot, state.snapshot);
}

sSupervisoryStateSnapshot cSupervisoryStateMachineAPI::snapshot() const
{
    return smContext_->snapshot;
}

void cSupervisoryStateMachineAPI::refreshSnapshotState()
{
    switch (smMachine_->getState())
    {
        case cSupervisoryController::State::Idle:
        {
            smContext_->snapshot.controlState = ecSupervisoryControlState::Idle;
            break;
        }

        case cSupervisoryController::State::Dispatching:
        {
            smContext_->snapshot.controlState = ecSupervisoryControlState::Dispatching;
            break;
        }

        case cSupervisoryController::State::MovingUp:
        {
            smContext_->snapshot.controlState = ecSupervisoryControlState::MovingUp;
            break;
        }

        case cSupervisoryController::State::MovingDown:
        {
            smContext_->snapshot.controlState = ecSupervisoryControlState::MovingDown;
            break;
        }

        case cSupervisoryController::State::Arrived:
        {
            smContext_->snapshot.controlState = ecSupervisoryControlState::Arrived;
            break;
        }

        case cSupervisoryController::State::Faulted:
        {
            smContext_->snapshot.controlState = ecSupervisoryControlState::Faulted;
            break;
        }
    }
}

} // namespace project6::supervisory
