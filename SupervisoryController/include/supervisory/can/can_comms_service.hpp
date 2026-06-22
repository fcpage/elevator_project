/******************************************************************
* can_comms_service.hpp - CAN Service Header
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Declares header for CAN service
******************************************************************/

#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "CAN_protocol.h"
#include "supervisory/can/can_adapter.hpp"
#include "supervisory/common/event.hpp"
#include "supervisory/common/result.hpp"
#include "supervisory/common/spsc_queue.hpp"

namespace project6::supervisory
{

/** @brief COMMS worker lifecycle state. */
enum class ecCanCommsState
{
    Stopped,
    Running,
    Failed
};

/** @brief First detected COMMS failure. */
enum class ecCanCommsFaultReason
{
    None,
    InitializationFailed,
    ReceiveFailed,
    TransmitFailed,
    InboundQueueFull,
    OutboundQueueFull,
    CommsProgressTimeout,
    /** Expected node heartbeat replies did not arrive within the verification window. */
    NodeHeartbeatTimeout,
    /** A node explicitly reported heartbeat communication failure. */
    NodeHeartbeatError,
    ThreadFailed
};

/** @brief Value snapshot for operator diagnostics. */
struct sCanCommsHealthSnapshot
{
    /** Worker lifecycle state. */
    ecCanCommsState state = ecCanCommsState::Stopped;
    /** First detected failure. */
    ecCanCommsFaultReason faultReason = ecCanCommsFaultReason::None;
    /** Worker progress counter. */
    std::uint64_t commsProgress = 0;
    /** Frames read from SocketCAN. */
    std::uint64_t receivedFrameCount = 0;
    /** Events rejected by a full queue. */
    std::uint64_t droppedEventCount = 0;
    /** Frames written to SocketCAN. */
    std::uint64_t transmittedFrameCount = 0;
    /** Failed SocketCAN writes. */
    std::uint64_t transmitFailureCount = 0;
    /** Nodes expected to reply to the active heartbeat request. */
    std::uint8_t expectedNodeHbReplyMask = 0;
    /** Nodes that replied to the active or most recent heartbeat request. */
    std::uint8_t receivedNodeHbReplyMask = 0;
    /** Nodes missing when the latest heartbeat verification window expired. */
    std::uint8_t missedNodeHbReplyMask = 0;
    /** True while CONTROL is waiting for node heartbeat replies. */
    bool isNodeHbReplyWindowOpen = false;
};

/** @brief Lock-free data exchange between COMMS and CONTROL. */
struct sCanExchange
{
    /** COMMS-to-CONTROL events. */
    cSpscQueue<sSupervisoryEvent, 64> receivedEvents;
    /** COMMS-to-CONTROL node heartbeat messages. */
    cSpscQueue<sNodeHbMessage, 32> receivedNodeHbMessages;
    /** CONTROL-to-COMMS frames. */
    cSpscQueue<sCanFrame, 16> transmitFrames;

    /** Worker progress counter. */
    std::atomic<std::uint64_t> commsProgress{0};
    /** Frames read from SocketCAN. */
    std::atomic<std::uint64_t> receivedFrameCount{0};
    /** Events rejected by a full queue. */
    std::atomic<std::uint64_t> droppedEventCount{0};
    /** Frames written to SocketCAN. */
    std::atomic<std::uint64_t> transmittedFrameCount{0};
    /** Failed SocketCAN writes. */
    std::atomic<std::uint64_t> transmitFailureCount{0};
    /** Current worker state. */
    std::atomic<ecCanCommsState> commsState{ecCanCommsState::Stopped};
    /** First detected failure. */
    std::atomic<ecCanCommsFaultReason> faultReason{ecCanCommsFaultReason::None};
};

/** @brief Inits and manages the SocketCAN and its worker thread. */
class cCanCommsService
{
public:
    /** @brief Creates a stopped COMMS service. */
    cCanCommsService(const sSocketCanConfig& config, sCanExchange& exchange);
    /** @brief Stops and joins the worker. */
    ~cCanCommsService();

    /** @brief Prevent the CommsService from being copied. */
    cCanCommsService(const cCanCommsService&) = delete;
    cCanCommsService& operator=(const cCanCommsService&) = delete;

    /** @brief Opens the configured SocketCAN adapter. */
    [[nodiscard]] ecOperationStatus initializeService();
    /** @brief Starts the COMMS worker. */
    [[nodiscard]] ecOperationStatus start();
    /** @brief Requests stop and joins the worker. */
    void stop();

private:
    /** @brief Runs bounded receive and transmit batches. */
    void run(std::stop_token stopToken) const noexcept;

    /** Sole SocketCAN owner. */
    cSocketCanAdapter adapter_;
    /** Shared CONTROL exchange. */
    sCanExchange& exchange_;
    /** COMMS worker thread. */
    std::jthread worker_;
    /** True after adapter initialization. */
    bool isInitialized_ = false;
};

} // namespace project6::supervisory
