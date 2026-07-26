/******************************************************************
* main.cpp - Supervisory Controller Entry Point
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Runs the supervisory controller service.
******************************************************************/

/** @file main.cpp - launches and runs the main supervisory application
 *  @brief Configures SocketCAN and runs the timed supervisory event loop.
*/

#include "supervisory/app/supervisory_application.hpp"
#include "supervisory/can/runtime_can_service.hpp"
#include "supervisory/database/database_message_service.hpp"
#ifdef SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
#include "supervisory/sim/simulator_diagnostics.hpp"
#endif

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <thread>

namespace
{

constexpr std::uint32_t kDefaultCanBitrateBitsPerSecond = 125000;
constexpr std::uint32_t kDefaultCanRestartMs = 100;

#ifdef SUPERVISORY_USE_VIRTUAL_CAN
constexpr const char* kDefaultCanInterfaceName = "vcan0";
#else
constexpr const char* kDefaultCanInterfaceName = "can0";
#endif

#if defined(SUPERVISORY_CAN_INTERFACE_PRECONFIGURED) || defined(SUPERVISORY_USE_VIRTUAL_CAN)
constexpr bool kConfigureCanInterfaceOnInitialize = false;
#else
constexpr bool kConfigureCanInterfaceOnInitialize = true;
#endif

volatile std::sig_atomic_t keepRunning = 1;

#ifdef SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
void applySimulatorDatabaseEnvironment(project6::supervisory::sDBServiceConfig& config)
{
    if (const char* value = std::getenv("ELEVATOR_DB_URL"); value != nullptr && value[0] != '\0')
    {
        config.url = value;
    }
    if (const char* value = std::getenv("ELEVATOR_DB_USER"); value != nullptr && value[0] != '\0')
    {
        config.user = value;
    }
    if (const char* value = std::getenv("ELEVATOR_DB_PASSWORD"); value != nullptr)
    {
        config.password = value;
    }
    if (const char* value = std::getenv("ELEVATOR_DB_SCHEMA"); value != nullptr && value[0] != '\0')
    {
        config.database = value;
    }
}

bool snapshotsDiffer(
    const project6::supervisory::sSupervisoryStateSnapshot& left,
    const project6::supervisory::sSupervisoryStateSnapshot& right)
{
    return left.controlState != right.controlState ||
           left.currentFloor != right.currentFloor ||
           left.targetFloor != right.targetFloor ||
           left.direction != right.direction ||
           left.isDoorOpen != right.isDoorOpen ||
           left.isFaulted != right.isFaulted;
}
#endif

void handleShutdownSignal(const int signalNumber)
{
    static_cast<void>(signalNumber);
    keepRunning = 0;
}

} // namespace

/**
 * @brief Starts the supervisory controller and maintains its 10 ms loop pace.
 *
 * The optional first argument overrides the default SocketCAN interface. Loop
 * elapsed time is measured with a monotonic clock and passed into the
 * application so door and movement timers reflect real scheduling delays.
 *
 * @param argumentCount Number of command-line arguments.
 * @param arguments Argument vector; arguments[1] may name a CAN interface.
 * @return Zero after signal-driven shutdown, or one after initialization or
 *         runtime failure.
 */
int main(const int argumentCount, char* arguments[])
{
    using namespace project6::supervisory;

    const char* interfaceName = kDefaultCanInterfaceName;
    if (argumentCount > 1 && arguments[1] != nullptr && arguments[1][0] != '\0')
    {
        interfaceName = arguments[1];
    }

    const sSocketCanConfig canConfig{
        interfaceName,
        kDefaultCanBitrateBitsPerSecond,
        kDefaultCanRestartMs,
        kConfigureCanInterfaceOnInitialize};
    sDBServiceConfig dbConfig{};
#ifdef SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
    applySimulatorDatabaseEnvironment(dbConfig);
#endif

    sDBMessageExchange dbExchange;
    sCanExchange canExchange;
    cRuntimeCanService commsService(canConfig, canExchange);
    cSupervisoryApplication application(canExchange, dbExchange);
    cDBMessageService database(dbConfig, dbExchange);
#ifdef SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
    cSimulatorDiagnosticsPublisher diagnostics;
    diagnostics.start();
#endif

    if (const ecOperationStatus status = commsService.initializeService(); status != ecOperationStatus::Ok)
    {
        std::cerr << "supervisory_controller: initialization failed: "
                  << operationStatusMessage(status) << '\n';
        return 1;
    }

    if (const ecOperationStatus status = commsService.start(); status != ecOperationStatus::Ok)
    {
        std::cerr << "supervisory_controller: COMMS start failed: "
                  << operationStatusMessage(status) << '\n';
        return 1;
    }
#ifdef SUPERVISORY_ENABLE_SIM_TESTPOINTS
    SUPERVISORY_TESTPOINT("service.can.started", "CAN worker is running");
#endif

    if (const ecOperationStatus status = database.start(); status != ecOperationStatus::Ok)
    {
        std::cerr << "supervisory_controller: database service start failed: "
                  << operationStatusMessage(status) << '\n';
        return 1;
    }
#ifdef SUPERVISORY_ENABLE_SIM_TESTPOINTS
    SUPERVISORY_TESTPOINT("service.database.started", "database worker is running");
#endif

    std::signal(SIGINT, handleShutdownSignal);
    std::signal(SIGTERM, handleShutdownSignal);

    constexpr std::chrono::milliseconds kLoopPeriodMs{10}; 
    auto previousIteration = std::chrono::steady_clock::now();
    auto nextIteration = previousIteration;
#ifdef SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
    std::uint64_t loopSequence = 0;
    std::chrono::milliseconds diagnosticElapsed{100};
    sSupervisoryStateSnapshot lastDiagnosticSnapshot = application.snapshot();
#endif

    std::clog << "START transport="
#ifdef SUPERVISORY_USE_SIMULATOR_CAN
              << "simulator-loopback"
#else
              << "socketcan interface=" << canConfig.interfaceName
              << " bitrate=" << canConfig.bitrateBitsPerSecond
#endif
              << '\n';

    while (keepRunning != 0)
    {
        const auto iterationStart = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            iterationStart - previousIteration);
        previousIteration = iterationStart;
        nextIteration += kLoopPeriodMs;

        if (const ecOperationStatus runStatus = application.runControlCycle(elapsedMs);
            runStatus != ecOperationStatus::Ok)
        {
            std::cerr << "supervisory_controller: runtime failed: "
                      << operationStatusMessage(runStatus) << '\n';
            return 1;
        }

        const auto iterationEnd = std::chrono::steady_clock::now();
        [[maybe_unused]] std::chrono::milliseconds overrunMs{0};
        if (iterationEnd < nextIteration)
        {
            std::this_thread::sleep_until(nextIteration);
        }
        else
        {
            overrunMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                iterationEnd - nextIteration);
#ifndef SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
            std::clog << "LOOP_OVERRUN elapsed_ms=" << overrunMs.count() << '\n';
#endif
            nextIteration = iterationEnd;
        }

#ifdef SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
        ++loopSequence;
        diagnosticElapsed += elapsedMs;
        const sSupervisoryStateSnapshot currentSnapshot = application.snapshot();
        if (diagnosticElapsed >= std::chrono::milliseconds{100} ||
            snapshotsDiffer(currentSnapshot, lastDiagnosticSnapshot))
        {
            static_cast<void>(diagnostics.tryPublish(makeSimulatorDiagnosticRecord(
                loopSequence,
                static_cast<std::uint64_t>(elapsedMs.count()),
                static_cast<std::uint64_t>(overrunMs.count()),
                application,
                dbExchange)));
            diagnosticElapsed = std::chrono::milliseconds{0};
            lastDiagnosticSnapshot = currentSnapshot;
        }
#endif
    }

    commsService.stop();
#ifdef SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
    diagnostics.stop();
#endif
    std::clog << "SHUTDOWN reason=signal\n";
    return 0;
}
