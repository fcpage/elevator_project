/******************************************************************
* supervisory_application.hpp - Supervisory Application Orchestrator
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @file supervisory_application.hpp
* @brief Declares the top-level event loop owner for the controller service.
******************************************************************/

#pragma once

#include <chrono>
#include <cstdint>

#include "supervisory/can/can_comms_service.hpp"
#include "supervisory/common/result.hpp"
#include "supervisory/control/supervisory_state_machine.hpp"

namespace project6::supervisory
{

/**
 * @brief Runs one bounded iteration of the deterministic CONTROL loop.
 */
class cSupervisoryApplication
{
public:
    /** @brief Creates CONTROL around the COMMS exchange. */
    explicit cSupervisoryApplication(sCanExchange& exchange);

    /**
     * @brief Applies bounded queued input, advances time, and publishes output.
     * @param elapsedMs Monotonic time elapsed since the previous loop iteration.
     * @return Ok. Communication failures are converted into state-machine faults
     *         without stopping CONTROL execution.
     */
    ecOperationStatus runControlCycle(std::chrono::milliseconds elapsedMs);

    /** @brief Returns the current control state. */
    [[nodiscard]] sSupervisoryStateSnapshot snapshot() const;
    /** @brief Returns current CAN health counters. */
    [[nodiscard]] sCanCommsHealthSnapshot canHealth() const;

private:
    /** @brief Moves a generated frame to COMMS. */
    void publishPendingFrame();
    /** @brief Detects COMMS failure or timeout. */
    void checkCommsHealth(std::chrono::milliseconds elapsedMs);
    /** @brief Latches a COMMS fault into CONTROL. */
    void faultComms(ecCanCommsFaultReason reason);

    /** Shared COMMS exchange. */
    sCanExchange& exchange_;
    /** CONTROL-owned state machine. */
    cSupervisoryStateMachineAPI appStateMachine_;
    /** Last observed heartbeat. */
    std::uint64_t lastHeartbeat_ = 0;
    /** Last observed dropped-event count. */
    std::uint64_t lastDroppedEventCount_ = 0;
    /** Last observed transmit-failure count. */
    std::uint64_t lastTransmitFailureCount_ = 0;
    /** Time without heartbeat progress. */
    std::chrono::milliseconds staleHeartbeatElapsed_{0};
    /** Prevents duplicate fault events. */
    bool isCommsFaultLatched_ = false;
};

} // namespace project6::supervisory
