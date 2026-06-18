/******************************************************************
* supervisory_state_machine.cpp - ESE Supervisory State Machine
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Holds the ESE-readable state-machine shape and public API.
******************************************************************/

/**
 * @file supervisory_state_machine.cpp
 * @brief Implements the event-driven supervisory control state machine for the elevator.
 *
 * This file coordinates queued floor requests, elevator movement, arrival and door timing,
 * and fault handling. It translates state transitions into CAN commands and exposes the
 * resulting high-level elevator state through the public supervisory state-machine API.
 */

#include "supervisory/control/supervisory_state_machine.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>

#include "supervisory/can/can_protocol.hpp"
#include "supervisory/drivers/supervisory_drivers.hpp"
#include "supervisory/scheduler/request_scheduler.hpp"

namespace project6::supervisory
{

/**
 * @brief Mutable data shared by generated conditions and transition actions.
 *
 * The generated ESE machine contains only control flow. Requests, timers,
 * hardware access, and externally visible state live here so callbacks can
 * remain small and the public API can expose a stable snapshot.
 */
struct sStateMachineContext
{
    /** Queues normalized requests until the machine enters Dispatching. */
    cRequestScheduler scheduler;

    /** Request currently being dispatched, moved toward, or completed. */
    std::optional<sElevatorRequest> activeRequest;

    /** Stable state representation returned through the public API. */
    sSupervisoryStateSnapshot snapshot{};

    /** Accumulated open-door time used by the Arrived timeout condition. */
    std::chrono::milliseconds doorOpenElapsedMs{0};

    /** Accumulated travel time used only by optional simulated auto-arrival. */
    std::chrono::milliseconds movementElapsedMs{0};

    /** Latched when elevator status confirms the active target floor. */
    bool didElevatorReportArrival = false;

    /** CAN command waiting for the application to hand to the COMMS thread. */
    std::optional<sCanFrame> pendingCanFrame;
};

namespace
{

/** Door dwell time before the Arrived state releases the serviced request. */
constexpr std::chrono::milliseconds kDoorOpenDurationMs{3000};
constexpr std::chrono::seconds kTravelTimeoutMs{10};
#ifdef SUPERVISORY_ENABLE_AUTO_ARRIVAL
constexpr std::chrono::seconds kSimulatedTravelDuration{3};
#endif

/**
 * @brief Context bridge used by no-argument callbacks emitted by the ESE generator.
 *
 * The current generator calls free functions and cannot pass user data. The API
 * therefore installs its owned context while alive. This design permits only one
 * active supervisory machine per process.
 */
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

/**
 * @brief Promotes the scheduler's highest-priority request into active service.
 *
 * Selection occurs only on entry to Dispatching, preserving the active target
 * until arrival or fault handling completes.
 */
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

/**
 * @brief Derives travel direction and transmits the active floor command.
 *
 * A CAN transmission failure is converted into the latched fault state consumed
 * by the next generated-machine update.
 */
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

    state->pendingCanFrame = makeSupervisorCommandFrame(state->activeRequest->floor, true);
    if (!state->pendingCanFrame.has_value())
    {
        state->snapshot.isFaulted = true;
    }
}

/**
 * @brief Commits arrival state and starts the door dwell timer.
 *
 * The active request remains present during Arrived so the completed target is
 * retained until the exit action closes the door and clears the request.
 */
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

/**
 * @brief Completes the active request when leaving Arrived.
 *
 * Door-close failure is latched as a fault for the following machine update.
 */
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

/** @brief Applies the supervisor's conservative stop behavior after a fault. */
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


/**
 * Represents a supervisory controller for managing the states and operations
 * of an elevator system.
 *
 * The `cSupervisoryController` class orchestrates transitions between different
 * operational states of an elevator. It manages requests, monitors conditions,
 * and handles state-specific logic to ensure proper functionality. This includes
 * handling faults, determining state transitions based on inputs or events, and
 * performing actions associated with entering and exiting states.
 */


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

    /**
     * Handles state updates while in the Idle state.
     *
     * This method checks specific conditions to determine whether a state
     * transition is required. If a fault is detected, it transitions to the
     * Faulted state. If a pending request is available, it transitions to
     * the Dispatching state.
     *
     * Conditions checked:
     * - Fault detection: Calls `conditionFaultDetected` to determine if a fault
     *   has occurred. If true, the state transitions to Faulted.
     * - Pending request availability: Calls `conditionPendingRequestAvailable`
     *   to check if there is a pending request. If true, the state transitions
     *   to Dispatching.
     *
     * Transitions:
     * - To `State::Faulted` if a fault is detected.
     * - To `State::Dispatching` if a pending request is available.
     */
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

    /**
     * Checks if there is a pending request available for processing.
     *
     * @return true if a pending request exists, false otherwise.
     */
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

    /**
     * Transitions the state machine into the "Idle" state.
     *
     * This method is responsible for initializing or performing the necessary
     * actions when entering the "Idle" state of the supervisory state machine.
     * The "Idle" state represents the default resting state after initialization
     * or after completing a request. In this state, the system awaits new requests
     * or events to react to.
     *
     * Intended to be invoked when a state transition to "Idle" is triggered.
     * Typically called by the state machine's transition logic.
     */
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

    /**
     * Handles the exit behavior for the Faulted state in the supervisory state machine.
     *
     * This method is executed when transitioning out of the Faulted state. It provides
     * a place to perform any necessary cleanup or logging specific to exiting the Faulted state.
     *
     * The Faulted state represents a situation where a fault has been detected, and this method
     * will be called during the transition to a new state following fault recovery or resolution.
     */
    void exitFaulted()
    {
    }

};

// #ESE-GENERATED-END: SupervisoryController


namespace
{

    /**
     * Retrieves the name of a specified supervisory control state as a string.
     *
     * This function converts an enumerated supervisory control state (`ecSupervisoryControlState`)
     * to its corresponding textual representation. The returned string represents the
     * operational state of the elevator system's supervisory control mechanism.
     *
     * @param state The supervisory control state to be converted to a string.
     *              Possible values are:
     *              - `ecSupervisoryControlState::Idle`: The elevator is idle and awaiting commands.
     *              - `ecSupervisoryControlState::Dispatching`: The elevator is dispatching to a floor.
     *              - `ecSupervisoryControlState::MovingUp`: The elevator is moving upward.
     *              - `ecSupervisoryControlState::MovingDown`: The elevator is moving downward.
     *              - `ecSupervisoryControlState::Arrived`: The elevator has arrived at the intended floor.
     *              - `ecSupervisoryControlState::Faulted`: The elevator system has encountered a fault.
     *
     * @return A constant character pointer representing the name of the state. Returns "Unknown"
     *         if the state does not match any known value in `ecSupervisoryControlState`.
     */
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

    /**
     *
     */
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

    /**
     *
     */
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

    /**
     *
     */
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

    /**
     *
     */
    bool isMoving(const ecSupervisoryControlState state)
{
    return state == ecSupervisoryControlState::MovingUp ||
           state == ecSupervisoryControlState::MovingDown;
}

} // namespace

cSupervisoryStateMachineAPI::cSupervisoryStateMachineAPI()
    : smContext_(std::make_unique<sStateMachineContext>())
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
        if (!state.didElevatorReportArrival && state.movementElapsedMs >= kSimulatedTravelDuration)
        {
            state.didElevatorReportArrival = true;
            std::clog << "AUTO_ARRIVAL target_floor="
                      << static_cast<unsigned int>(state.snapshot.targetFloor)
                      << " elapsed_ms=" << state.movementElapsedMs.count() << '\n';
        }
#endif
    }
    
    if (!state.didElevatorReportArrival && state.movementElapsedMs >= kTravelTimeoutMs)
    {
        state.snapshot.isFaulted = true;
        std::cerr << "Timeout occurred. The target floor was: "
                  << static_cast<unsigned int>(state.snapshot.targetFloor)
                  << " elapsed_ms=" << state.movementElapsedMs.count() << '\n';
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

std::optional<sCanFrame> cSupervisoryStateMachineAPI::tryTakePendingCanFrame()
{
    std::optional<sCanFrame> frame = smContext_->pendingCanFrame;
    smContext_->pendingCanFrame.reset();
    return frame;
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
