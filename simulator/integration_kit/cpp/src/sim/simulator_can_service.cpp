/******************************************************************
* simulator_can_service.cpp - Native localhost simulator CAN service
* @brief Bridges the production SA's real CAN exchange to the Python plant.
******************************************************************/

#include "supervisory/can/can_protocol.hpp"
#include "supervisory/sim/simulator_can_service.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace project6::supervisory
{

namespace
{

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void closeSocket(const SocketHandle value) { if (value != kInvalidSocket) closesocket(value); }
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void closeSocket(const SocketHandle value) { if (value != kInvalidSocket) ::close(value); }
#endif

std::optional<int> extractInteger(const std::string& line, const char* key)
{
    const std::regex expression(
        std::string{"\""} + key + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(line, match, expression))
    {
        return std::nullopt;
    }
    return std::stoi(match[1].str(), nullptr, 10);
}

std::optional<std::string> environmentValue(const char* name)
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t valueLength = 0;
    if (_dupenv_s(&value, &valueLength, name) != 0 || value == nullptr)
    {
        std::free(value);
        return std::nullopt;
    }
    const std::string result{value};
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::nullopt : std::optional<std::string>{value};
#endif
}

std::optional<sCanFrame> parseCanRx(const std::string& line)
{
    if (line.find("\"type\":\"can_rx\"") == std::string::npos)
    {
        return std::nullopt;
    }
    const std::optional<int> id = extractInteger(line, "id");
    if (!id.has_value() || *id < 0 || *id > 0x7FF)
    {
        return std::nullopt;
    }

    const std::regex dataExpression{"\"data\"\\s*:\\s*\\[([^\\]]*)\\]"};
    std::smatch dataMatch;
    if (!std::regex_search(line, dataMatch, dataExpression))
    {
        return std::nullopt;
    }

    sCanFrame frame{};
    frame.id = static_cast<std::uint16_t>(*id);
    std::stringstream bytes{dataMatch[1].str()};
    std::string token;
    while (std::getline(bytes, token, ',') && frame.dataLength < kCanPayloadLength)
    {
        try
        {
            const int value = std::stoi(token, nullptr, 0);
            if (value < 0 || value > 255)
            {
                return std::nullopt;
            }
            frame.data[frame.dataLength++] = static_cast<std::uint8_t>(value);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
    return frame;
}

SocketHandle connectLoopback(const char* host, const std::uint16_t port)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    const std::string portString = std::to_string(port);
    if (::getaddrinfo(host, portString.c_str(), &hints, &addresses) != 0)
    {
        return kInvalidSocket;
    }
    SocketHandle connected = kInvalidSocket;
    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next)
    {
        const SocketHandle candidate =
            ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == kInvalidSocket)
        {
            continue;
        }
        if (::connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
        {
            connected = candidate;
            break;
        }
        closeSocket(candidate);
    }
    ::freeaddrinfo(addresses);
    return connected;
}

bool sendAll(const SocketHandle socketHandle, const std::string& value)
{
    std::size_t offset = 0;
    while (offset < value.size())
    {
#ifdef _WIN32
        const int result = ::send(
            socketHandle,
            value.data() + offset,
            static_cast<int>(value.size() - offset),
            0);
#else
        const ssize_t result =
            ::send(socketHandle, value.data() + offset, value.size() - offset, MSG_NOSIGNAL);
#endif
        if (result <= 0)
        {
            return false;
        }
        offset += static_cast<std::size_t>(result);
    }
    return true;
}

std::string encodeCanTx(const sCanFrame& frame)
{
    std::ostringstream output;
    output << "{\"version\":1,\"type\":\"can_tx\",\"id\":" << frame.id << ",\"data\":[";
    for (std::uint8_t index = 0; index < frame.dataLength; ++index)
    {
        if (index != 0)
        {
            output << ',';
        }
        output << static_cast<unsigned int>(frame.data[index]);
    }
    output << "]}\n";
    return output.str();
}

} // namespace

class cSimulatorCanService::Impl
{
public:
    explicit Impl(sCanExchange& exchange) : exchange_(exchange) {}
    ~Impl() { stop(); }

    ecOperationStatus start()
    {
        const std::optional<std::string> host = environmentValue("ELEVATOR_SIM_CAN_HOST");
        const std::optional<std::string> portText = environmentValue("ELEVATOR_SIM_CAN_PORT");
        if (!host.has_value() || !portText.has_value())
        {
            return ecOperationStatus::InvalidArgument;
        }
        const long port = std::strtol(portText->c_str(), nullptr, 10);
        if (port <= 0 || port > 65535)
        {
            return ecOperationStatus::InvalidArgument;
        }
        host_ = *host;
        port_ = static_cast<std::uint16_t>(port);
        worker_ = std::jthread([this](const std::stop_token token) { run(token); });
        return ecOperationStatus::Ok;
    }

    void stop()
    {
        if (worker_.joinable())
        {
            worker_.request_stop();
            worker_.join();
        }
    }

private:
    void run(const std::stop_token stopToken) noexcept
    {
        try
        {
#ifdef _WIN32
            WSADATA winsockData{};
            if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0)
            {
                exchange_.faultReason.store(ecCanCommsFaultReason::InitializationFailed);
                exchange_.commsState.store(ecCanCommsState::Failed);
                return;
            }
#endif
            SocketHandle socketHandle = kInvalidSocket;
            for (int attempt = 0; attempt < 50 && !stopToken.stop_requested(); ++attempt)
            {
                socketHandle = connectLoopback(host_.c_str(), port_);
                if (socketHandle != kInvalidSocket)
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
            }
            if (socketHandle == kInvalidSocket)
            {
                exchange_.faultReason.store(ecCanCommsFaultReason::InitializationFailed);
                exchange_.commsState.store(ecCanCommsState::Failed);
#ifdef _WIN32
                WSACleanup();
#endif
                return;
            }
            if (!sendAll(socketHandle, "{\"version\":1,\"type\":\"hello\",\"role\":\"sa\"}\n"))
            {
                closeSocket(socketHandle);
                exchange_.commsState.store(ecCanCommsState::Failed);
#ifdef _WIN32
                WSACleanup();
#endif
                return;
            }

            exchange_.commsState.store(ecCanCommsState::Running);
            std::string input;
            while (!stopToken.stop_requested())
            {
                bool didWork = false;
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(socketHandle, &readSet);
                timeval timeout{};
                timeout.tv_usec = 1000;
#ifdef _WIN32
                const int selected = ::select(0, &readSet, nullptr, nullptr, &timeout);
#else
                const int selected = ::select(socketHandle + 1, &readSet, nullptr, nullptr, &timeout);
#endif
                if (selected < 0)
                {
                    break;
                }
                if (selected > 0 && FD_ISSET(socketHandle, &readSet))
                {
                    char buffer[2048];
#ifdef _WIN32
                    const int received = ::recv(socketHandle, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
                    const ssize_t received = ::recv(socketHandle, buffer, sizeof(buffer), 0);
#endif
                    if (received <= 0)
                    {
                        break;
                    }
                    input.append(buffer, static_cast<std::size_t>(received));
                    std::size_t newline = 0;
                    while ((newline = input.find('\n')) != std::string::npos)
                    {
                        const std::string line = input.substr(0, newline);
                        input.erase(0, newline + 1);
                        const std::optional<sCanFrame> frame = parseCanRx(line);
                        if (!frame.has_value())
                        {
                            continue;
                        }
                        didWork = true;
                        exchange_.receivedFrameCount.fetch_add(1);
                        if (const std::optional<sNodeHbMessage> heartbeat = decodeNodeHbFrame(*frame);
                            heartbeat.has_value())
                        {
                            if (!exchange_.receivedNodeHbMessages.tryPush(*heartbeat))
                            {
                                exchange_.droppedEventCount.fetch_add(1);
                            }
                            continue;
                        }
                        const std::optional<sDecodedCanMessage> decoded = decodeCanFrame(*frame);
                        if (!decoded.has_value())
                        {
                            continue;
                        }
                        const std::optional<sSupervisoryEvent> event = toSupervisoryEvent(*decoded);
                        if (event.has_value() && !exchange_.receivedEvents.tryPush(*event))
                        {
                            exchange_.droppedEventCount.fetch_add(1);
                        }
                    }
                }

                for (std::size_t count = 0; count < 16; ++count)
                {
                    sCanFrame frame{};
                    if (!exchange_.transmitFrames.tryPop(frame))
                    {
                        break;
                    }
                    if (!sendAll(socketHandle, encodeCanTx(frame)))
                    {
                        exchange_.transmitFailureCount.fetch_add(1);
                        exchange_.faultReason.store(ecCanCommsFaultReason::TransmitFailed);
                        exchange_.commsState.store(ecCanCommsState::Failed);
                        closeSocket(socketHandle);
#ifdef _WIN32
                        WSACleanup();
#endif
                        return;
                    }
                    didWork = true;
                    exchange_.transmittedFrameCount.fetch_add(1);
                }
                exchange_.commsProgress.fetch_add(1);
                if (!didWork)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
            }
            closeSocket(socketHandle);
            exchange_.commsState.store(ecCanCommsState::Stopped);
#ifdef _WIN32
            WSACleanup();
#endif
        }
        catch (...)
        {
            exchange_.faultReason.store(ecCanCommsFaultReason::ThreadFailed);
            exchange_.commsState.store(ecCanCommsState::Failed);
        }
    }

    sCanExchange& exchange_;
    std::jthread worker_;
    std::string host_;
    std::uint16_t port_ = 0;
};

cSimulatorCanService::cSimulatorCanService(
    const sSocketCanConfig& config,
    sCanExchange& exchange)
    : impl_(std::make_unique<Impl>(exchange))
{
    static_cast<void>(config);
}

cSimulatorCanService::~cSimulatorCanService() = default;

ecOperationStatus cSimulatorCanService::initializeService()
{
    return ecOperationStatus::Ok;
}

ecOperationStatus cSimulatorCanService::start()
{
    return impl_->start();
}

void cSimulatorCanService::stop()
{
    impl_->stop();
}

} // namespace project6::supervisory
