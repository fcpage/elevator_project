/******************************************************************
* can_adapter.hpp - CAN Adapter Boundary
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @file can_adapter.hpp
* @brief Declares the boundary between supervisor logic and CAN hardware.
******************************************************************/

#pragma once

#include <cstdint>
#include "supervisory/can/can_frame.hpp"
#include "supervisory/common/result.hpp"

namespace project6::supervisory
{

/**
 * @brief Runtime setup for the Linux SocketCAN network interface.
 *
 * SocketCAN configures physical bus timing through the Linux network device.
 * Separate from CanProtocolConfig, which describes only CAN IDs and payload layout.
 */
struct sSocketCanConfig
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
     * `sudo ip link set can0 up type can bitrate 125000`
     * as a backup.
     */
    std::uint32_t bitrateBitsPerSecond = 125000;

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
 * The adapter owns one Linux raw CAN socket and translates between the Linux
 * can_frame representation and the platform-independent sCanFrame model. It
 * does not interpret project protocol bits.
 */
class cSocketCanAdapter
{
public:
    /**
     * @brief Creates an adapter with explicit SocketCAN runtime configuration.
     *
     * @param config Linux interface setup used by initialize().
     */
    explicit cSocketCanAdapter(const sSocketCanConfig &config);

    /**
     * @brief Creates an adapter for the named SocketCAN network interface.
     *
     * @param interfaceName Null-terminated interface name, such as "can0".
     */
    explicit cSocketCanAdapter(const char* interfaceName);

    /**
     * @brief Closes the owned socket when running on Linux.
     */
    ~cSocketCanAdapter();

    cSocketCanAdapter(const cSocketCanAdapter&) = delete;
    cSocketCanAdapter& operator=(const cSocketCanAdapter&) = delete;

    /**
     * @brief Opens and configures the CAN interface.
     *
     * When requested, initialization first invokes Linux `ip link` commands,
     * then resolves the interface index, binds a PF_CAN raw socket, and enables
     * non-blocking reads.
     *
     * @return ecOperationStatus::Ok on success; otherwise a configuration,
     *         privilege, or hardware availability status.
     */
    [[nodiscard]] ecOperationStatus initialize();

    /**
     * @brief Attempts to read one CAN frame without blocking.
     *
     * Linux CAN flags are stripped so the returned identifier contains only the
     * standard 11-bit node ID.
     *
     * @param frame Receives the complete frame when one is available.
     * @return Ok when frame was populated, WouldBlock when no frame is pending,
     *         or an initialization/hardware failure status.
     */
    [[nodiscard]] ecOperationStatus tryReadFrame(sCanFrame& frame) const;

    /**
     * @brief Sends one CAN frame.
     *
     * @param frame Frame to transmit on the configured CAN interface.
     * @return ecOperationStatus::Ok when the complete Linux frame is written;
     *         otherwise an initialization, validation, or hardware status.
     */
    [[nodiscard]] ecOperationStatus sendFrame(const sCanFrame& frame) const;

private:
    /** Sentinel used when no Linux socket is owned. */
    static constexpr int kInvalidSocket = -1;

    /** Interface and physical bus settings applied during initialize(). */
    sSocketCanConfig socketConfig_{};

    /** Owned PF_CAN socket descriptor, or kInvalidSocket before initialization. */
    int socketSocketFd_ = kInvalidSocket;

    /** True only after socket creation, binding, and non-blocking setup succeed. */
    bool socketIsInitialized_ = false;
};

} // namespace project6::supervisory
