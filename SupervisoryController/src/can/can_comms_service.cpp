/******************************************************************
* can_comms_service.cpp - CAN Service Implementation
* Author: Project 6 Team
* Last Modified: 2026-06-11
* @brief Implementation of CAN service functions
******************************************************************/

#include "supervisory/can/can_comms_service.hpp"

#include <chrono>
#include <optional>

#include "supervisory/can/can_protocol.hpp"

namespace project6::supervisory
{

namespace
{

constexpr std::size_t kMaximumReadsPerCycle = 32;
constexpr std::size_t kMaximumWritesPerCycle = 16;
constexpr std::chrono::milliseconds kIdleDelay{1}; // Not a timeout! See application file.

void recordFault(sCanExchange& exchange, const ecCanCommsFaultReason reason)
{
    ecCanCommsFaultReason expected = ecCanCommsFaultReason::None;
    static_cast<void>(exchange.faultReason.compare_exchange_strong(expected, reason));
}

} // namespace

cCanCommsService::cCanCommsService(const sSocketCanConfig& config, sCanExchange& exchange)
    : adapter_(config), exchange_(exchange)
{
}

cCanCommsService::~cCanCommsService()
{
    stop();
}

ecOperationStatus cCanCommsService::initializeService()
{
    const ecOperationStatus status = adapter_.initialize();
    isInitialized_ = status == ecOperationStatus::Ok;
    if (!isInitialized_)
    {
        recordFault(exchange_, ecCanCommsFaultReason::InitializationFailed);
        exchange_.commsState.store(ecCanCommsState::Failed);
    }
    return status;
}

ecOperationStatus cCanCommsService::start()
{
    if (!isInitialized_)
    {
        return ecOperationStatus::NotInitialized;
    }
    if (worker_.joinable())
    {
        return ecOperationStatus::InvalidArgument;
    }

    worker_ = std::jthread([this](const std::stop_token stopToken) {
        run(stopToken);
    });
    return ecOperationStatus::Ok;
}

void cCanCommsService::stop()
{
    if (worker_.joinable())
    {
        worker_.request_stop();
        worker_.join();
    }

    if (exchange_.commsState.load() != ecCanCommsState::Failed)
    {
        exchange_.commsState.store(ecCanCommsState::Stopped);
    }
}

void cCanCommsService::run(const std::stop_token stopToken) const noexcept
{
    try
    {
        exchange_.commsState.store(ecCanCommsState::Running);
        std::optional<sCanFrame> pendingTransmit;

        while (!stopToken.stop_requested())
        {
            bool didWork = false;

            for (std::size_t count = 0; count < kMaximumReadsPerCycle; ++count)
            {
                sCanFrame frame{};
                const ecOperationStatus status = adapter_.tryReadFrame(frame);
                if (status == ecOperationStatus::WouldBlock)
                {
                    break;
                }
                if (status != ecOperationStatus::Ok)
                {
                    recordFault(exchange_, ecCanCommsFaultReason::ReceiveFailed);
                    exchange_.commsState.store(ecCanCommsState::Failed);
                    return;
                }

                didWork = true;
                exchange_.receivedFrameCount.fetch_add(1);
                const std::optional<sDecodedCanMessage> message = decodeCanFrame(frame);
                if (!message.has_value())
                {
                    continue;
                }

                if (const std::optional<sSupervisoryEvent> event = toSupervisoryEvent(*message);
                    event.has_value() && !exchange_.receivedEvents.tryPush(*event))
                {
                    exchange_.droppedEventCount.fetch_add(1);
                    recordFault(exchange_, ecCanCommsFaultReason::InboundQueueFull);
                }
            }

            for (std::size_t count = 0; count < kMaximumWritesPerCycle; ++count)
            {
                if (!pendingTransmit.has_value())
                {
                    sCanFrame frame{};
                    if (!exchange_.transmitFrames.tryPop(frame))
                    {
                        break;
                    }
                    pendingTransmit = frame;
                }

                const ecOperationStatus status = adapter_.sendFrame(*pendingTransmit);
                if (status == ecOperationStatus::WouldBlock)
                {
                    break;
                }
                if (status != ecOperationStatus::Ok)
                {
                    exchange_.transmitFailureCount.fetch_add(1);
                    recordFault(exchange_, ecCanCommsFaultReason::TransmitFailed);
                    exchange_.commsState.store(ecCanCommsState::Failed);
                    return;
                }

                didWork = true;
                pendingTransmit.reset();
                exchange_.transmittedFrameCount.fetch_add(1);
            }

            exchange_.heartbeat.fetch_add(1);
            if (!didWork)
            {
                std::this_thread::sleep_for(kIdleDelay);
            }
        }

        exchange_.commsState.store(ecCanCommsState::Stopped);
    }
    catch (...)
    {
        recordFault(exchange_, ecCanCommsFaultReason::ThreadFailed);
        exchange_.commsState.store(ecCanCommsState::Failed);
    }
}

} // namespace project6::supervisory
