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
#include "supervisory/can/can_comms_service.hpp"

#ifdef SUPERVISORY_ENABLE_DEMO_MODES
#include "supervisory/demo/demo_control.hpp"
#endif

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

const char* operationStatusMessage(const project6::supervisory::ecOperationStatus status)
{
    using project6::supervisory::ecOperationStatus;

    switch (status)
    {
        case ecOperationStatus::Ok:
            return "operation completed successfully";
        case ecOperationStatus::NotInitialized:
            return "a required module was not initialized";
        case ecOperationStatus::InvalidArgument:
            return "invalid runtime configuration";
        case ecOperationStatus::WouldBlock:
            return "operation would block";
        case ecOperationStatus::InsufficientPrivileges:
            return "permission denied while configuring CAN; run as root or grant CAP_NET_ADMIN";
        case ecOperationStatus::HardwareUnavailable:
            return "required hardware or SocketCAN interface is unavailable";
        case ecOperationStatus::NetworkUnavailable:
            return "required network service is unavailable";
        case ecOperationStatus::NotImplemented:
            return "requested operation is not implemented";
    }

    return "unknown operation status";
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
    sCanExchange canExchange;
    cCanCommsService commsService(canConfig, canExchange);
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
    cSupervisoryApplication application(canExchange, ecNodeHbFailureMode::FaultControl, &announcementService);

#ifdef SUPERVISORY_ENABLE_DEMO_MODES
    const std::string demoControlPath =
        environmentValue("SUPERVISORY_DEMO_CONTROL_FILE", "demo_control.txt");
    cDemoControl demoControl({
        demoControlPath.c_str()});
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

    if (!announcementService.start())
    {
        std::cerr << "supervisory_controller: announcement service start failed\n";
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

#ifdef SUPERVISORY_ENABLE_DEMO_MODES
        // Temporary Phase 2 substitute for the unfinished GUI/database path.
        // The adapter emits the same normalized events a future DB worker must use.
        demoControl.poll(application);
#endif

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
    announcementService.stop();
    std::clog << "SHUTDOWN reason=signal\n";
    return 0;
}
