/******************************************************************
* supervisory_application.hpp - Supervisory Application Orchestrator
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Declares the top-level event loop owner for the controller service.
******************************************************************/

#pragma once

#include <chrono>

#include "project6/supervisory/can/can_adapter.hpp"
#include "project6/supervisory/common/result.hpp"
#include "project6/supervisory/control/supervisory_state_machine.hpp"
#include "project6/supervisory/http/http_server.hpp"

namespace project6::supervisory
{

/**
 * @brief Owns startup and one iteration of the supervisory event loop.
 *
 * Keep blocking I/O out of this class. Adapters should provide non-blocking
 * polling methods so timer, HTTP, and CAN work can share one predictable loop.
 */
class SupervisoryApplication
{
public:
    SupervisoryApplication(SocketCanAdapter& canAdapter, HttpServer& httpServer);

    OperationStatus initialize();
    OperationStatus runOnce(std::chrono::milliseconds elapsedMs);

private:
    void pollCan();
    void pollHttp();
    void processTimer(std::chrono::milliseconds elapsedMs);

    SocketCanAdapter& canAdapter_;
    HttpServer& httpServer_;
    SupervisoryStateMachineAPI stateMachine_;
    bool isInitialized_ = false;
};

} // namespace project6::supervisory
