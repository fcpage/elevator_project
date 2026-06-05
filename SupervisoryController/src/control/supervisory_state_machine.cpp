/******************************************************************
* supervisory_state_machine.cpp - ESE Supervisory State Machine
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Holds the ESE-readable state-machine shape and public API.
******************************************************************/

#include "project6/supervisory/control/supervisory_state_machine.hpp"

#include "project6/supervisory/drivers/supervisory_drivers.hpp"
#include "project6/supervisory/scheduler/request_scheduler.hpp"

#include <chrono>
#include <memory>
#include <optional>

namespace project6::supervisory
{

struct StateMachineContext
{
    RequestScheduler scheduler;
    std::optional<ElevatorRequest> activeRequest;
    SupervisoryStateSnapshot snapshot{};
    std::chrono::milliseconds doorOpenElapsedMs{0};
    bool didElevatorReportArrival = false;
};

namespace
{

constexpr std::chrono::milliseconds kDoorOpenDurationMs{3000};
StateMachineContext* activeContext = nullptr;

StateMachineContext* context()
{
    return activeContext;
}

bool hasPendingRequest()
{
    StateMachineContext* state = context();
    if (state == nullptr)
    {
        return false;
    }

    return state->activeRequest.has_value() || state->scheduler.hasPendingRequest();
}

bool targetIsCurrentFloor()
{
    StateMachineContext* state = context();
    if (state == nullptr || !state->activeRequest.has_value())
    {
        return false;
    }

    return state->activeRequest->floor == state->snapshot.currentFloor;
}

bool targetAboveCurrentFloor()
{
    StateMachineContext* state = context();
    if (state == nullptr || !state->activeRequest.has_value())
    {
        return false;
    }

    return state->activeRequest->floor > state->snapshot.currentFloor;
}

bool targetBelowCurrentFloor()
{
    StateMachineContext* state = context();
    if (state == nullptr || !state->activeRequest.has_value())
    {
        return false;
    }

    return state->activeRequest->floor < state->snapshot.currentFloor;
}

bool elevatorReportedArrival()
{
    StateMachineContext* state = context();
    if (state == nullptr)
    {
        return false;
    }

    return state->didElevatorReportArrival;
}

bool doorWaitExpired()
{
    StateMachineContext* state = context();
    if (state == nullptr)
    {
        return false;
    }

    return state->doorOpenElapsedMs >= kDoorOpenDurationMs;
}

bool faultDetected()
{
    StateMachineContext* state = context();
    if (state == nullptr)
    {
        return false;
    }

    return state->snapshot.isFaulted;
}

void selectNextRequest()
{
    StateMachineContext* state = context();
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
    StateMachineContext* state = context();
    if (state == nullptr || !state->activeRequest.has_value())
    {
        return;
    }

    if (state->activeRequest->floor > state->snapshot.currentFloor)
    {
        state->snapshot.direction = TravelDirection::Up;
    }
    else if (state->activeRequest->floor < state->snapshot.currentFloor)
    {
        state->snapshot.direction = TravelDirection::Down;
    }
    else
    {
        state->snapshot.direction = TravelDirection::None;
    }

    const OperationStatus status = drivers::commandElevatorToFloor(state->activeRequest->floor);
    if (status != OperationStatus::Ok)
    {
        state->snapshot.isFaulted = true;
    }
}

void recordArrival()
{
    StateMachineContext* state = context();
    if (state == nullptr)
    {
        return;
    }

    if (state->activeRequest.has_value())
    {
        state->snapshot.currentFloor = state->activeRequest->floor;
        state->snapshot.targetFloor = state->activeRequest->floor;
    }

    state->snapshot.direction = TravelDirection::None;
    state->snapshot.isDoorOpen = true;
    state->doorOpenElapsedMs = std::chrono::milliseconds{0};
    state->didElevatorReportArrival = false;

    const OperationStatus status = drivers::commandDoorOpen();
    if (status != OperationStatus::Ok)
    {
        state->snapshot.isFaulted = true;
    }
}

void clearServicedRequest()
{
    StateMachineContext* state = context();
    if (state == nullptr)
    {
        return;
    }

    state->activeRequest.reset();
    state->snapshot.direction = TravelDirection::None;
    state->snapshot.isDoorOpen = false;
    state->doorOpenElapsedMs = std::chrono::milliseconds{0};

    const OperationStatus status = drivers::commandDoorClose();
    if (status != OperationStatus::Ok)
    {
        state->snapshot.isFaulted = true;
    }
}

void enterFaultState()
{
    StateMachineContext* state = context();
    if (state == nullptr)
    {
        return;
    }

    state->snapshot.direction = TravelDirection::None;
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

class SupervisoryController
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

    SupervisoryController()
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

SupervisoryStateMachineAPI::SupervisoryStateMachineAPI()
    : context_(std::make_unique<StateMachineContext>())
{
    activeContext = context_.get();
    machine_ = std::make_unique<SupervisoryController>();
    refreshSnapshotState();
}

SupervisoryStateMachineAPI::~SupervisoryStateMachineAPI()
{
    if (activeContext == context_.get())
    {
        activeContext = nullptr;
    }
}

void SupervisoryStateMachineAPI::handleEvent(const SupervisoryEvent& event)
{
    StateMachineContext& state = *context_;

    if (event.type == EventType::HttpFloorRequest ||
        event.type == EventType::CanFloorRequest ||
        event.type == EventType::CanCarRequest)
    {
        state.scheduler.enqueueEvent(event);
    }

    if (event.reportedFloor.has_value())
    {
        state.snapshot.currentFloor = *event.reportedFloor;
    }

    if (event.reportedDirection != TravelDirection::None)
    {
        state.snapshot.direction = event.reportedDirection;
    }

    if (event.type == EventType::CanElevatorStatus &&
        state.activeRequest.has_value() &&
        event.reportedFloor.has_value() &&
        *event.reportedFloor == state.activeRequest->floor)
    {
        state.didElevatorReportArrival = true;
    }

    if (event.type == EventType::TimerTick && state.snapshot.isDoorOpen)
    {
        state.doorOpenElapsedMs += event.timestampMs;
    }

    if (event.type == EventType::Fault)
    {
        state.snapshot.isFaulted = true;
    }

    machine_->update();
    refreshSnapshotState();
}

SupervisoryStateSnapshot SupervisoryStateMachineAPI::snapshot() const
{
    return context_->snapshot;
}

void SupervisoryStateMachineAPI::refreshSnapshotState()
{
    switch (machine_->getState())
    {
        case SupervisoryController::State::Idle:
        {
            context_->snapshot.controlState = SupervisoryControlState::Idle;
            break;
        }

        case SupervisoryController::State::Dispatching:
        {
            context_->snapshot.controlState = SupervisoryControlState::Dispatching;
            break;
        }

        case SupervisoryController::State::MovingUp:
        {
            context_->snapshot.controlState = SupervisoryControlState::MovingUp;
            break;
        }

        case SupervisoryController::State::MovingDown:
        {
            context_->snapshot.controlState = SupervisoryControlState::MovingDown;
            break;
        }

        case SupervisoryController::State::Arrived:
        {
            context_->snapshot.controlState = SupervisoryControlState::Arrived;
            break;
        }

        case SupervisoryController::State::Faulted:
        {
            context_->snapshot.controlState = SupervisoryControlState::Faulted;
            break;
        }
    }
}

} // namespace project6::supervisory
