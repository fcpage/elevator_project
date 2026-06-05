/******************************************************************
* socket_can_adapter.cpp - SocketCAN Adapter
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @brief Provides the Linux SocketCAN/USB-CAN adapter boundary.
******************************************************************/

#include "project6/supervisory/can/can_adapter.hpp"

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

OperationStatus runProcess(const std::array<const char*, 10>& arguments)
{
    const pid_t childProcessId = ::fork();
    if (childProcessId < 0)
    {
        return OperationStatus::HardwareUnavailable;
    }

    if (childProcessId == 0)
    {
        ::execvp(arguments[0], const_cast<char* const*>(arguments.data()));
        _exit(127);
    }

    int status = 0;
    if (::waitpid(childProcessId, &status, 0) < 0)
    {
        return OperationStatus::HardwareUnavailable;
    }

    if (!didProcessExitSuccessfully(status))
    {
        if (::geteuid() != 0)
        {
            return OperationStatus::InsufficientPrivileges;
        }

        return OperationStatus::HardwareUnavailable;
    }

    return OperationStatus::Ok;
}

OperationStatus configureSocketCanInterface(const SocketCanConfig& config)
{
    const std::string bitrate = std::to_string(config.bitrateBitsPerSecond);
    const std::string restartMs = std::to_string(config.restartMs);

    const std::array<const char*, 10> downCommand{
        "ip",
        "link",
        "set",
        config.interfaceName,
        "down",
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr};

    OperationStatus status = runProcess(downCommand);
    if (status != OperationStatus::Ok)
    {
        return status;
    }

    const std::array<const char*, 10> upCommand{
        "ip",
        "link",
        "set",
        config.interfaceName,
        "up",
        "type",
        "can",
        "bitrate",
        bitrate.c_str(),
        nullptr};

    status = runProcess(upCommand);
    if (status != OperationStatus::Ok)
    {
        return status;
    }

    const std::array<const char*, 10> restartCommand{
        "ip",
        "link",
        "set",
        config.interfaceName,
        "type",
        "can",
        "restart-ms",
        restartMs.c_str(),
        nullptr,
        nullptr};

    return runProcess(restartCommand);
}
#endif

} // namespace

SocketCanAdapter::SocketCanAdapter(SocketCanConfig config)
    : config_(config)
{
}

SocketCanAdapter::SocketCanAdapter(const char* interfaceName)
    : config_(SocketCanConfig{interfaceName})
{
}

SocketCanAdapter::~SocketCanAdapter()
{
#ifdef __linux__
    if (socketFd_ != kInvalidSocket)
    {
        static_cast<void>(::close(socketFd_));
    }
#endif
}

OperationStatus SocketCanAdapter::initialize()
{
    if (config_.interfaceName == nullptr || config_.bitrateBitsPerSecond == 0)
    {
        return OperationStatus::InvalidArgument;
    }

#ifndef __linux__
    return OperationStatus::HardwareUnavailable;
#else
    if (config_.configureInterfaceOnInitialize)
    {
        const OperationStatus configureStatus = configureSocketCanInterface(config_);
        if (configureStatus != OperationStatus::Ok)
        {
            return configureStatus;
        }
    }

    socketFd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socketFd_ == kInvalidSocket)
    {
        return OperationStatus::HardwareUnavailable;
    }

    ifreq interfaceRequest{};
    std::strncpy(interfaceRequest.ifr_name, config_.interfaceName, IFNAMSIZ - 1);
    if (::ioctl(socketFd_, SIOCGIFINDEX, &interfaceRequest) < 0)
    {
        return OperationStatus::HardwareUnavailable;
    }

    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = interfaceRequest.ifr_ifindex;
    if (::bind(socketFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        return OperationStatus::HardwareUnavailable;
    }

    const int flags = ::fcntl(socketFd_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(socketFd_, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        return OperationStatus::HardwareUnavailable;
    }

    isInitialized_ = true;
    return OperationStatus::Ok;
#endif
}

std::optional<CanFrame> SocketCanAdapter::tryReadFrame() const
{
    if (!isInitialized_)
    {
        return std::nullopt;
    }

#ifndef __linux__
    return std::nullopt;
#else
    can_frame linuxFrame{};
    const ssize_t bytesRead = ::read(socketFd_, &linuxFrame, sizeof(linuxFrame));
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

    CanFrame frame{};
    frame.id = static_cast<std::uint16_t>(linuxFrame.can_id & CAN_SFF_MASK);
    frame.dataLength = linuxFrame.can_dlc;

    for (std::uint8_t index = 0; index < frame.dataLength && index < kCanPayloadLength; ++index)
    {
        frame.data[index] = linuxFrame.data[index];
    }

    return frame;
#endif
}

OperationStatus SocketCanAdapter::sendFrame(const CanFrame& frame) const
{
    if (!isInitialized_)
    {
        return OperationStatus::NotInitialized;
    }

    if (frame.dataLength > kCanPayloadLength)
    {
        return OperationStatus::InvalidArgument;
    }

    if (frame.id > kStandardCanIdMax)
    {
        return OperationStatus::InvalidArgument;
    }

#ifndef __linux__
    return OperationStatus::HardwareUnavailable;
#else
    can_frame linuxFrame{};
    linuxFrame.can_id = frame.id;
    linuxFrame.can_dlc = frame.dataLength;

    for (std::uint8_t index = 0; index < frame.dataLength; ++index)
    {
        linuxFrame.data[index] = frame.data[index];
    }

    const ssize_t bytesWritten = ::write(socketFd_, &linuxFrame, sizeof(linuxFrame));
    if (bytesWritten != static_cast<ssize_t>(sizeof(linuxFrame)))
    {
        return OperationStatus::HardwareUnavailable;
    }

    return OperationStatus::Ok;
#endif
}

} // namespace project6::supervisory
