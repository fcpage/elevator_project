/******************************************************************
 * main.cpp - Supervisory Controller Entry Point
 * Author: Project 6 Team
 * Last Modified: 2026-06-04
 * @brief Wires the top-level supervisor objects for runtime startup.
 ******************************************************************/

#include "project6/supervisory/app/supervisory_application.hpp"

#include <chrono>
#include <iostream>

namespace {

    const char *operationStatusMessage(const project6::supervisory::OperationStatus status) {
        using project6::supervisory::OperationStatus;

        switch (status) {
            case OperationStatus::Ok:
                return "operation completed successfully";
            case OperationStatus::NotInitialized:
                return "a required module was not initialized";
            case OperationStatus::InvalidArgument:
                return "invalid runtime configuration";
            case OperationStatus::WouldBlock:
                return "operation would block";
            case OperationStatus::InsufficientPrivileges:
                return "permission denied while configuring CAN; run as root or grant "
                       "CAP_NET_ADMIN";
            case OperationStatus::HardwareUnavailable:
                return "required hardware or SocketCAN interface is unavailable";
            case OperationStatus::NetworkUnavailable:
                return "required network service is unavailable";
            case OperationStatus::NotImplemented:
                return "requested operation is not implemented";
        }

        return "unknown operation status";
    }

} // namespace

int main() {
    using namespace project6::supervisory;

    constexpr SocketCanConfig canConfig{"can0", 250000, 100, true};
    SocketCanAdapter canAdapter(canConfig);
    HttpServer httpServer(HttpServerConfig{});
    SupervisoryApplication application(canAdapter, httpServer);

    const OperationStatus status = application.initialize();
    if (status != OperationStatus::Ok) {
        std::cerr << "supervisory_controller: initialization failed: " << operationStatusMessage(status) << '\n';
        return 1;
    }

    static constexpr std::chrono::milliseconds kLoopPeriodMs{10};
    static_cast<void>(application.runOnce(kLoopPeriodMs));

    return 0;
}
