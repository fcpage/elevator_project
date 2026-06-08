/******************************************************************
* supervisory_application.hpp - Supervisory Application Orchestrator
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Declares the top-level event loop owner for the controller service.
******************************************************************/

#pragma once

#include <chrono>

#include "project6/supervisory/can/can_adapter.hpp"
#include "project6/supervisory/common/result.hpp"
#include "project6/supervisory/control/supervisory_state_machine.hpp"

namespace project6::supervisory
{

/**
 * @brief Owns startup and one iteration of the supervisory event loop.
 *
 * Keep blocking I/O out of this class. Adapters should provide non-blocking
 * polling methods so timer, HTTP, and CAN work can share one predictable loop.
 */
class cSupervisoryApplication
{
public:
    /**
     * @brief Creates the supervisor around the active CAN adapter.
     */
    explicit cSupervisoryApplication(cSocketCanAdapter& canAdapter);

    /**
     * @brief Initializes runtime hardware dependencies.
     */
    ecOperationStatus initialize();

    /**
     * @brief Processes pending inputs and advances controller time once.
     */
    ecOperationStatus runLoopOnce(std::chrono::milliseconds elapsedMs);

private:
    void pollCan();
    void processTimer(std::chrono::milliseconds elapsedMs);

    cSocketCanAdapter& appCanAdapter_;
    cSupervisoryStateMachineAPI appStateMachine_;
    bool appIsInitialized_ = false;
};

} // namespace project6::supervisory
