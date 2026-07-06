/******************************************************************
* supervisory_state_machine.hpp - Supervisory State Machine Boundary
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Declares the narrow API around the ESE-generated state machine.
******************************************************************/

#pragma once

#include <cstdint>
#include <memory>

#include "project6/supervisory/common/event.hpp"

namespace project6::supervisory
{

class cSupervisoryController;
class cSocketCanAdapter;
struct sStateMachineContext;

/**
 * @brief Public high-level state names reported by the supervisory state machine.
 *
 * These values mirror the major ESE states while keeping generated state-machine
 * details out of the rest of the program.
 */
enum class ecSupervisoryControlState
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
struct sSupervisoryStateSnapshot
{
    ecSupervisoryControlState controlState = ecSupervisoryControlState::Idle;
    std::uint8_t currentFloor = 1;
    std::uint8_t targetFloor = 1;
    ecTravelDirection direction = ecTravelDirection::None;
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
class cSupervisoryStateMachineAPI
{
public:
    /**
     * @brief Creates the state machine around the active CAN adapter.
     */
    explicit cSupervisoryStateMachineAPI(cSocketCanAdapter& canAdapter);

    /**
     * @brief Releases the state-machine context.
     */
    ~cSupervisoryStateMachineAPI();

    cSupervisoryStateMachineAPI(const cSupervisoryStateMachineAPI&) = delete;
    cSupervisoryStateMachineAPI& operator=(const cSupervisoryStateMachineAPI&) = delete;

    /**
     * @brief Applies one normalized event and advances the ESE machine once.
     *
     * @param event Event produced by CAN, timer, or fault handling code.
     */
    void handleEvent(const sSupervisoryEvent& event);

    /**
     * @brief Returns the latest public state snapshot.
     */
    [[nodiscard]] sSupervisoryStateSnapshot snapshot() const;

private:
    void refreshSnapshotState();

    std::unique_ptr<sStateMachineContext> smContext_;
    std::unique_ptr<cSupervisoryController> smMachine_;
};

} // namespace project6::supervisory
