/******************************************************************
* socket_can_adapter.cpp - SocketCAN Adapter
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Provides the Linux SocketCAN/USB-CAN adapter boundary.
******************************************************************/

#include "project6/supervisory/can/can_adapter.hpp"

//#define __linux__  // Uncomment for Windows Compilation Testing

#ifdef __linux__
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace project6::supervisory
{

namespace
{

constexpr std::uint16_t kStandardCanIdMax = 0x7FF;

#ifdef __linux__
bool didProcessExitSuccessfully(const int status)
{
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

ecOperationStatus runProcess(const std::array<const char*, 11>& arguments)
{
    const pid_t childProcessId = ::fork();
    if (childProcessId < 0)
    {
        return ecOperationStatus::HardwareUnavailable;
    }

    if (childProcessId == 0)
    {
        ::execvp(arguments[0], const_cast<char* const*>(arguments.data()));
        _exit(127);
    }

    int status = 0;
    if (::waitpid(childProcessId, &status, 0) < 0)
    {
        return ecOperationStatus::HardwareUnavailable;
    }

    if (!didProcessExitSuccessfully(status))
    {
        if (::geteuid() != 0)
        {
            return ecOperationStatus::InsufficientPrivileges;
        }

        return ecOperationStatus::HardwareUnavailable;
    }

    return ecOperationStatus::Ok;
}

ecOperationStatus configureSocketCanInterface(const sSocketCanConfig& config)
{
    const std::string bitrate = std::to_string(config.bitrateBitsPerSecond);
    const std::string restartMs = std::to_string(config.restartMs);

    const std::array<const char*, 11> downCommand{
        "ip",
        "link",
        "set",
        config.interfaceName,
        "down",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr};

    ecOperationStatus status = runProcess(downCommand);
    if (status != ecOperationStatus::Ok)
    {
        return status;
    }

    const std::array<const char*, 11> configureCommand{
        "ip",
        "link",
        "set",
        config.interfaceName,
        "type",
        "can",
        "bitrate",
        bitrate.c_str(),
        "restart-ms",
        restartMs.c_str(),
        nullptr};

    status = runProcess(configureCommand);
    if (status != ecOperationStatus::Ok)
    {
        return status;
    }

    const std::array<const char*, 11> upCommand{
        "ip",
        "link",
        "set",
        config.interfaceName,
        "up",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr};

    return runProcess(upCommand);
}
#endif

} // namespace

cSocketCanAdapter::cSocketCanAdapter(const sSocketCanConfig &config)
    : socketConfig_(config)
{
}

cSocketCanAdapter::cSocketCanAdapter(const char* interfaceName)
    : socketConfig_(sSocketCanConfig{interfaceName})
{
}

cSocketCanAdapter::~cSocketCanAdapter()
{
#ifdef __linux__
    if (socketSocketFd_ != kInvalidSocket)
    {
        static_cast<void>(::close(socketSocketFd_));
    }
#endif
}

/**
 * @brief Initializes a CAN Socket Adapter class by opening a socket on the linux hardware
 *
 * @return Enum class status of operation
 */
ecOperationStatus cSocketCanAdapter::initialize()
{
    if (socketConfig_.interfaceName == nullptr || socketConfig_.bitrateBitsPerSecond == 0)
    {
        return ecOperationStatus::InvalidArgument;
    }

#ifndef __linux__
    return ecOperationStatus::HardwareUnavailable;
#else
    if (socketConfig_.configureInterfaceOnInitialize)
    {
        const ecOperationStatus configureStatus = configureSocketCanInterface(socketConfig_);
        if (configureStatus != ecOperationStatus::Ok)
        {
            return configureStatus;
        }
    }

    socketSocketFd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socketSocketFd_ == kInvalidSocket)
    {
        return ecOperationStatus::HardwareUnavailable;
    }

    ifreq interfaceRequest{};
    std::strncpy(interfaceRequest.ifr_name, socketConfig_.interfaceName, IFNAMSIZ - 1);
    if (::ioctl(socketSocketFd_, SIOCGIFINDEX, &interfaceRequest) < 0)
    {
        return ecOperationStatus::HardwareUnavailable;
    }

    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = interfaceRequest.ifr_ifindex;
    if (::bind(socketSocketFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        return ecOperationStatus::HardwareUnavailable;
    }

    const int flags = ::fcntl(socketSocketFd_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(socketSocketFd_, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        return ecOperationStatus::HardwareUnavailable;
    }

    socketIsInitialized_ = true;
    return ecOperationStatus::Ok;
#endif
}

std::optional<sCanFrame> cSocketCanAdapter::tryReadFrame() const
{
    if (!socketIsInitialized_)
    {
        return std::nullopt;
    }

#ifndef __linux__
    return std::nullopt;
#else
    can_frame linuxFrame{};
    const ssize_t bytesRead = ::read(socketSocketFd_, &linuxFrame, sizeof(linuxFrame));
    if (bytesRead < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return std::nullopt;
        }

        return std::nullopt;
    }

    if (bytesRead != static_cast<ssize_t>(sizeof(linuxFrame)))
    {
        return std::nullopt;
    }

    sCanFrame frame{};
    frame.id = static_cast<std::uint16_t>(linuxFrame.can_id & CAN_SFF_MASK);
    frame.dataLength = linuxFrame.can_dlc;

    for (std::uint8_t index = 0; index < frame.dataLength && index < kCanPayloadLength; ++index)
    {
        frame.data[index] = linuxFrame.data[index];
    }

    return frame;
#endif
}

ecOperationStatus cSocketCanAdapter::sendFrame(const sCanFrame& frame) const
{
    if (!socketIsInitialized_)
    {
        return ecOperationStatus::NotInitialized;
    }

    if (frame.dataLength > kCanPayloadLength)
    {
        return ecOperationStatus::InvalidArgument;
    }

    if (frame.id > kStandardCanIdMax)
    {
        return ecOperationStatus::InvalidArgument;
    }

#ifndef __linux__
    return ecOperationStatus::HardwareUnavailable;
#else
    can_frame linuxFrame{};
    linuxFrame.can_id = frame.id;
    linuxFrame.can_dlc = frame.dataLength;

    for (std::uint8_t index = 0; index < frame.dataLength; ++index)
    {
        linuxFrame.data[index] = frame.data[index];
    }

    const ssize_t bytesWritten = ::write(socketSocketFd_, &linuxFrame, sizeof(linuxFrame));
    if (bytesWritten != static_cast<ssize_t>(sizeof(linuxFrame)))
    {
        return ecOperationStatus::HardwareUnavailable;
    }

    return ecOperationStatus::Ok;
#endif
}

} // namespace project6::supervisory
