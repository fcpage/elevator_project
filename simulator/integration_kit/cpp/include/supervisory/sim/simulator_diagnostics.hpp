/******************************************************************
* simulator_diagnostics.hpp - Best-effort simulator diagnostics
* @brief Publishes read-only CONTROL/COMMS/DATABASE snapshots to localhost.
******************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <thread>

#include "supervisory/can/can_comms_service.hpp"
#include "supervisory/common/spsc_queue.hpp"
#include "supervisory/control/supervisory_state_machine.hpp"
#include "supervisory/database/database_message_service.hpp"

namespace project6::supervisory
{

class cSupervisoryApplication;

struct sSimulatorDiagnosticRecord
{
    std::uint64_t loopSequence = 0;
    std::uint64_t loopElapsedMs = 0;
    std::uint64_t loopOverrunMs = 0;
    sSupervisoryStateSnapshot control{};
    sCanCommsHealthSnapshot can{};
    ecDBServiceState databaseState = ecDBServiceState::Stopped;
    ecDBServiceFaultReason databaseFault = ecDBServiceFaultReason::None;
    std::uint64_t databaseReadCount = 0;
    std::uint64_t databaseDroppedEventCount = 0;
    std::uint64_t databaseWriteCount = 0;
    std::uint64_t databaseWriteFailureCount = 0;
};

struct sSimulatorTestpointRecord
{
    std::uint64_t sequence = 0;
    std::uint64_t threadId = 0;
    std::array<char, 64> name{};
    std::array<char, 192> detail{};
};

/**
 * A diagnostic failure never changes CONTROL behavior. When disconnected or
 * backpressured, records are dropped and accounted for locally.
 */
class cSimulatorDiagnosticsPublisher
{
public:
    cSimulatorDiagnosticsPublisher();
    ~cSimulatorDiagnosticsPublisher();

    cSimulatorDiagnosticsPublisher(const cSimulatorDiagnosticsPublisher&) = delete;
    cSimulatorDiagnosticsPublisher& operator=(const cSimulatorDiagnosticsPublisher&) = delete;

    /** Starts only when ELEVATOR_SIM_DIAGNOSTICS_HOST/PORT are present. */
    void start();
    void stop();
    bool tryPublish(const sSimulatorDiagnosticRecord& record);
    bool tryPublishTestpoint(std::string_view name, std::string_view detail);
    [[nodiscard]] std::uint64_t droppedCount() const;
    [[nodiscard]] bool enabled() const;

private:
    void run(std::stop_token stopToken) noexcept;

    cSpscQueue<sSimulatorDiagnosticRecord, 128> records_;
    cSpscQueue<sSimulatorTestpointRecord, 128> testpoints_;
    std::mutex testpointProducerMutex_;
    std::jthread worker_;
    std::atomic<std::uint64_t> droppedCount_{0};
    std::atomic<std::uint64_t> testpointSequence_{0};
    const char* host_ = nullptr;
    std::uint16_t port_ = 0;
    bool enabled_ = false;
};

sSimulatorDiagnosticRecord makeSimulatorDiagnosticRecord(
    std::uint64_t loopSequence,
    std::uint64_t loopElapsedMs,
    std::uint64_t loopOverrunMs,
    const cSupervisoryApplication& application,
    const sDBMessageExchange& databaseExchange);

/**
 * Emits a non-blocking simulator breadcrumb from any SA thread.
 *
 * Returns false when simulator diagnostics are disabled or backpressured.
 */
bool trySimulatorTestpoint(std::string_view name, std::string_view detail = {});

} // namespace project6::supervisory

#ifdef SUPERVISORY_ENABLE_SIM_TESTPOINTS
#define SUPERVISORY_TESTPOINT(name, detail) \
    static_cast<void>(::project6::supervisory::trySimulatorTestpoint((name), (detail)))
#else
#define SUPERVISORY_TESTPOINT(name, detail) static_cast<void>(0)
#endif
