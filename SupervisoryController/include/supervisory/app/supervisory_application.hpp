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
#include "supervisory/database/database_message_service.hpp"

namespace project6::supervisory
{

/** @brief Car controller bit in the node heartbeat reply masks. */
inline constexpr std::uint8_t kNodeHbCcMask = 1u << 0;
/** @brief Floor 1 controller bit in the node heartbeat reply masks. */
inline constexpr std::uint8_t kNodeHbFc1Mask = 1u << 1;
/** @brief Floor 2 controller bit in the node heartbeat reply masks. */
inline constexpr std::uint8_t kNodeHbFc2Mask = 1u << 2;
/** @brief Floor 3 controller bit in the node heartbeat reply masks. */
inline constexpr std::uint8_t kNodeHbFc3Mask = 1u << 3;
/** @brief Nodes required to reply to a supervisory heartbeat request. */
inline constexpr std::uint8_t kExpectedNodeHbReplyMask =
    kNodeHbCcMask | kNodeHbFc1Mask | kNodeHbFc2Mask | kNodeHbFc3Mask;

/**
 * @brief Selects how CONTROL reacts when expected node heartbeat replies miss
 *        the verification window.
 */
enum class ecNodeHbFailureMode
{
    /** Report the missed nodes and fault the state machine. */
    FaultControl,
    /** Report the missed nodes without faulting the state machine. */
    LogOnly
};

/**
 * @brief Runs one bounded iteration of the deterministic CONTROL loop.
 */
class cSupervisoryApplication
{
public:
    /**
     * @brief Creates CONTROL around the COMMS exchange.
     *
     * @param exchange Shared queues and health counters owned by main().
     * @param nodeHbFailureMode Runtime action when scoped nodes miss a
     *        heartbeat verification window.
     */
    explicit cSupervisoryApplication(
        sCanExchange& canExchange,
        sDBMessageExchange& databaseExchange,
        ecNodeHbFailureMode nodeHbFailureMode = ecNodeHbFailureMode::FaultControl);

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
    /** @brief Moves a generated state machine snapshot to database. */
    void publishSnapshot();
    /** @brief Detects COMMS failure or timeout. */
    void checkCommsHealth(std::chrono::milliseconds elapsedMs);
    /** @brief Drains and schedules node heartbeat messages. */
    void processNodeHbCycle(std::chrono::milliseconds elapsedMs);
    /** @brief Applies one decoded node heartbeat message. */
    void handleNodeHbMessage(const sNodeHbMessage& message);
    /** @brief Opens a new reply window and publishes the SC heartbeat request. */
    void startNodeHbRequest();
    /** @brief Publishes a node heartbeat frame through the COMMS transmit queue. */
    void publishNodeHbFrame(ecNodeHb type);
    /** @brief Records an explicit node-reported heartbeat failure. */
    void reportNodeHbError(std::uint16_t sourceId);
    /**
     * @brief Records a node heartbeat miss and optionally faults CONTROL.
     *
     * Missed-node detail remains visible through canHealth() even when the
     * selected policy is LogOnly.
     */
    void reportNodeHbTimeout();
    /** @brief Latches a CONTROL fault without attempting COMMS restart. */
    void faultControl(ecCanCommsFaultReason reason);
    /** @brief Latches a COMMS fault into CONTROL. */
    void faultComms(ecCanCommsFaultReason reason);

    /** Shared COMMS exchange. */
    sCanExchange& canExchange_;
    /** Shared DATABASE exchange. */
    sDBMessageExchange& databaseExchange_;
    /** CONTROL-owned state machine. */
    cSupervisoryStateMachineAPI appStateMachine_;
    /** Last observed COMMS worker progress counter. */
    std::uint64_t lastCommsProgress_ = 0;
    /** Last observed dropped-event count. */
    std::uint64_t lastDroppedEventCount_ = 0;
    /** Last observed transmit-failure count. */
    std::uint64_t lastTransmitFailureCount_ = 0;
    /** Time without COMMS worker progress. */
    std::chrono::milliseconds staleCommsProgressElapsed_{0};
    /** Time accumulated toward the next outbound node heartbeat request. */
    std::chrono::milliseconds nodeHbIntervalElapsed_{0};
    /** Time accumulated while waiting for node heartbeat replies. */
    std::chrono::milliseconds nodeHbReplyWindowElapsed_{0};
    /** Nodes expected to reply to the active heartbeat request. */
    std::uint8_t expectedNodeHbReplyMask_ = 0;
    /** Nodes that have replied to the active or most recent heartbeat request. */
    std::uint8_t receivedNodeHbReplyMask_ = 0;
    /** Nodes missing when the latest heartbeat reply window expired. */
    std::uint8_t missedNodeHbReplyMask_ = 0;
    /** True while CONTROL is waiting for node heartbeat replies. */
    bool isNodeHbReplyWindowOpen_ = false;
    /** Runtime policy for missed node heartbeat replies. */
    ecNodeHbFailureMode nodeHbFailureMode_ = ecNodeHbFailureMode::FaultControl;
    /** Prevents duplicate fault events. */
    bool isControlFaultLatched_ = false;
};

} // namespace project6::supervisory
