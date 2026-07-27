/******************************************************************
* simulator_can_service.hpp - Localhost simulator CAN service
* @brief Provides the simulator transport behind the production SA entry point.
******************************************************************/

#pragma once

#include "supervisory/can/can_comms_service.hpp"
#include "supervisory/common/result.hpp"

#include <memory>

namespace project6::supervisory
{

/**
 * Simulator-only replacement for cCanCommsService.
 *
 * It exposes the same lifecycle used by main.cpp, but moves CAN frames over
 * the simulator's localhost NDJSON connection. CONTROL continues to use the
 * real sCanExchange queues and protocol decoder.
 */
class cSimulatorCanService
{
public:
    cSimulatorCanService(const sSocketCanConfig& config, sCanExchange& exchange);
    ~cSimulatorCanService();

    cSimulatorCanService(const cSimulatorCanService&) = delete;
    cSimulatorCanService& operator=(const cSimulatorCanService&) = delete;

    /** Validates the simulator connection environment. */
    ecOperationStatus initializeService();
    /** Starts the non-blocking bridge worker. */
    ecOperationStatus start();
    /** Stops and joins the bridge worker. */
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace project6::supervisory
