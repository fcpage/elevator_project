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
#include "supervisory/audio/announcement_service.hpp"
// Selects the production CAN service normally and the simulator bridge only
// when SUPERVISORY_BUILD_SIMULATOR is enabled by the simulator launcher.
#include "supervisory/can/runtime_can_service.hpp"
#include "supervisory/database/database_message_service.hpp"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <string>
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

std::string environmentValue(const char* name, const char* fallback)
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t valueLength = 0;
    if (_dupenv_s(&value, &valueLength, name) == 0 && value != nullptr)
    {
        const std::string result(value);
        std::free(value);
        return result;
    }
    std::free(value);
    return fallback;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? fallback : value;
#endif
}

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
    const std::string databaseUrl =
        environmentValue("ELEVATOR_DB_URL", "tcp://127.0.0.1:3306");
    const std::string databaseUser =
        environmentValue("ELEVATOR_DB_USER", "pi");
    const std::string databasePassword =
        environmentValue("ELEVATOR_DB_PASSWORD", "ese");
    const std::string databaseSchema =
        environmentValue("ELEVATOR_DB_SCHEMA", "elevatorg1");
    const sDBServiceConfig dbConfig{
        databaseUrl.c_str(),
        databaseUser,
        databasePassword,
        databaseSchema};
    sDBMessageExchange dbExchange;
    sCanExchange canExchange;
    // Keep the normal application entry point; only the transport is selected
    // at build time for a simulator run.
    cRuntimeCanService commsService(canConfig, canExchange);
    sAnnouncementExchange announcementExchange;
    const std::string audioDirectory =
        environmentValue("SUPERVISORY_AUDIO_DIR", "audio");
    const std::string audioDevice =
        environmentValue("SUPERVISORY_AUDIO_DEVICE", "Headphones");
    const sAnnouncementServiceConfig announcementConfig{
        true,
        audioDirectory.c_str(),
        audioDevice.c_str(),
        1.0F};
    cAnnouncementService announcementService(announcementConfig, announcementExchange);
    cSupervisoryApplication application(
        canExchange,
        dbExchange,
        ecNodeHbFailureMode::FaultControl,
        &announcementService);
    cDBMessageService database(dbConfig, dbExchange);

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

    if (!announcementService.start())
    {
        std::cerr << "supervisory_controller: announcement service start failed\n";
    }
    if (const ecOperationStatus status = database.start(); status != ecOperationStatus::Ok)
    {
        std::cerr << "supervisory_controller: database service start failed: "
                  << operationStatusMessage(status) << '\n';
        announcementService.stop();
        return 1;
    }

    std::signal(SIGINT, handleShutdownSignal);
    std::signal(SIGTERM, handleShutdownSignal);

    constexpr std::chrono::milliseconds kLoopPeriodMs{10}; 
    auto previousIteration = std::chrono::steady_clock::now();
    auto nextIteration = previousIteration;

    std::clog << "START interface=" << canConfig.interfaceName
              << " bitrate=" << canConfig.bitrateBitsPerSecond << '\n';

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

        if (const auto iterationEnd = std::chrono::steady_clock::now(); iterationEnd < nextIteration)
        {
            std::this_thread::sleep_until(nextIteration);
        }
        else
        {
            const auto overrunMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                iterationEnd - nextIteration);
            std::clog << "LOOP_OVERRUN elapsed_ms=" << overrunMs.count() << '\n';
            nextIteration = iterationEnd;
        }
    }

    commsService.stop();
    database.stop();
    announcementService.stop();
    std::clog << "SHUTDOWN reason=signal\n";
    return 0;
}
