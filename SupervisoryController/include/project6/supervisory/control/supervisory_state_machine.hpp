/******************************************************************
* supervisory_state_machine.hpp - Supervisory State Machine Boundary
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Declares the narrow API around the ESE-generated state machine.
******************************************************************/

#pragma once

#include <cstdint>
#include <memory>

#include "project6/supervisory/common/event.hpp"

namespace project6::supervisory
{

class SupervisoryController;
struct StateMachineContext;

/**
 * @brief Public high-level state names reported by the supervisory state machine.
 *
 * These values mirror the major ESE states while keeping generated state-machine
 * details out of the rest of the program.
 */
enum class SupervisoryControlState
{
    Idle,
    Dispatching,
    MovingUp,
    MovingDown,
    Arrived,
    Faulted
};

/**
 * @brief Snapshot of high-level control state for diagnostics and tests.
 *
 * This type is safe to expose to tests, simulator adapters, and status
 * endpoints. It intentionally avoids exposing generated ESE internals.
 */
struct SupervisoryStateSnapshot
{
    SupervisoryControlState controlState = SupervisoryControlState::Idle;
    std::uint8_t currentFloor = 1;
    std::uint8_t targetFloor = 1;
    TravelDirection direction = TravelDirection::None;
    bool isDoorOpen = false;
    bool isFaulted = false;
};

/**
 * @brief Public API for the ESE-backed supervisory state machine.
 *
 * The generated ESE machine stays in the corresponding C++ file.
 * Callers interact with this class by feeding normalized events and reading a
 * diagnostic snapshot.
 */
class SupervisoryStateMachineAPI
{
public:
    /**
     * @brief Creates the state-machine context and initializes the ESE machine.
     */
    SupervisoryStateMachineAPI();

    /**
     * @brief Releases the state-machine context.
     */
    ~SupervisoryStateMachineAPI();

    SupervisoryStateMachineAPI(const SupervisoryStateMachineAPI&) = delete;
    SupervisoryStateMachineAPI& operator=(const SupervisoryStateMachineAPI&) = delete;

    /**
     * @brief Applies one normalized event and advances the ESE machine once.
     *
     * @param event Event produced by CAN, HTTP, timer, or fault handling code.
     */
    void handleEvent(const SupervisoryEvent& event);

    /**
     * @brief Returns the latest public state snapshot.
     */
    [[nodiscard]] SupervisoryStateSnapshot snapshot() const;

private:
    void refreshSnapshotState();

    std::unique_ptr<StateMachineContext> context_;
    std::unique_ptr<SupervisoryController> machine_;
};

} // namespace project6::supervisory
