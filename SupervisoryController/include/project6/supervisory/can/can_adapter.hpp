/******************************************************************
* can_adapter.hpp - CAN Adapter Boundary
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @brief Declares the boundary between supervisor logic and CAN hardware.
******************************************************************/

#pragma once

#include <cstdint>
#include <optional>

#include "project6/supervisory/can/can_frame.hpp"
#include "project6/supervisory/common/result.hpp"

namespace project6::supervisory
{

/**
 * @brief Runtime setup for the Linux SocketCAN network interface.
 *
 * SocketCAN configures physical bus timing through the Linux network device.
 * Separate from CanProtocolConfig, which describes only CAN IDs and payload layout.
 */
struct SocketCanConfig
{
    /**
     * @brief SocketCAN interface name exposed by Linux, typically can0.
     */
    const char* interfaceName = "can0";

    /**
     * @brief Physical CAN bus bitrate used when initialize() configures Linux.
     *
     * This could fail without running the program with sufficient permissions.
     * Use the bash command:
     * `sudo ip link set can0 up type can bitrate 250000`
     * as a backup.
     */
    std::uint32_t bitrateBitsPerSecond = 250000;

    /**
     * @brief Automatic bus-off recovery delay passed to SocketCAN.
     *
     * A non-zero value helps the lab setup recover after wiring or termination
     * mistakes without restarting the supervisor process.
     */
    std::uint32_t restartMs = 100;

    /**
     * @brief Whether initialize() should configure the Linux network device.
     *
     * Disable this when a systemd unit or setup script creates the CAN interface
     * configuration before the supervisor starts.
     */
    bool configureInterfaceOnInitialize = true;
};

/**
 * @brief SocketCAN data used by the Raspberry Pi supervisory controller.
 *
 * The adapter is responsible for the OS CAN socket and moves
 * raw CanFrame values only.
 */
class SocketCanAdapter
{
public:
    /**
     * @brief Creates an adapter with explicit SocketCAN runtime configuration.
     *
     * @param config Linux interface setup used by initialize().
     */
    explicit SocketCanAdapter(SocketCanConfig config);

    /**
     * @brief Creates an adapter for the named SocketCAN network interface.
     *
     * @param interfaceName Null-terminated interface name, such as "can0".
     */
    explicit SocketCanAdapter(const char* interfaceName);

    /**
     * @brief Closes the owned socket when running on Linux.
     */
    ~SocketCanAdapter();

    SocketCanAdapter(const SocketCanAdapter&) = delete;
    SocketCanAdapter& operator=(const SocketCanAdapter&) = delete;

    /**
     * @brief Opens and configures the CAN interface.
     */
    OperationStatus initialize();

    /**
     * @brief Attempts to read one CAN frame without blocking indefinitely.
     *
     * @return A frame when one is available; otherwise std::nullopt.
     */
    [[nodiscard]] std::optional<CanFrame> tryReadFrame() const;

    /**
     * @brief Sends one CAN frame.
     *
     * @param frame Frame to transmit on the configured CAN interface.
     * @return OperationStatus::Ok when the frame is written successfully.
     */
    [[nodiscard]] OperationStatus sendFrame(const CanFrame& frame) const;

private:
    static constexpr int kInvalidSocket = -1;

    SocketCanConfig config_{};
    int socketFd_ = kInvalidSocket;
    bool isInitialized_ = false;
};

} // namespace project6::supervisory
