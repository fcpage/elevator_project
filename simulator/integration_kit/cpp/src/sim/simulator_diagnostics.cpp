/******************************************************************
* simulator_diagnostics.cpp - Best-effort simulator diagnostics
******************************************************************/

#include "supervisory/sim/simulator_diagnostics.hpp"
#include "supervisory/app/supervisory_application.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace project6::supervisory
{

namespace
{

std::atomic<cSimulatorDiagnosticsPublisher*> activePublisher{nullptr};

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void closeSocket(const SocketHandle socketHandle)
{
    if (socketHandle != kInvalidSocket)
    {
        closesocket(socketHandle);
    }
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void closeSocket(const SocketHandle socketHandle)
{
    if (socketHandle != kInvalidSocket)
    {
        ::close(socketHandle);
    }
}
#endif

const char* controlStateName(const ecSupervisoryControlState state)
{
    switch (state)
    {
        case ecSupervisoryControlState::Idle: return "Idle";
        case ecSupervisoryControlState::Dispatching: return "Dispatching";
        case ecSupervisoryControlState::MovingUp: return "MovingUp";
        case ecSupervisoryControlState::MovingDown: return "MovingDown";
        case ecSupervisoryControlState::Arrived: return "Arrived";
        case ecSupervisoryControlState::Faulted: return "Faulted";
    }
    return "Unknown";
}

const char* directionName(const ecTravelDirection direction)
{
    switch (direction)
    {
        case ecTravelDirection::None: return "None";
        case ecTravelDirection::Up: return "Up";
        case ecTravelDirection::Down: return "Down";
    }
    return "Unknown";
}

SocketHandle connectSocket(const char* host, const std::uint16_t port)
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

bool sendAll(const SocketHandle socketHandle, const std::string& payload)
{
    std::size_t sent = 0;
    while (sent < payload.size())
    {
#ifdef _WIN32
        const int result = ::send(
            socketHandle,
            payload.data() + sent,
            static_cast<int>(payload.size() - sent),
            0);
#else
        const ssize_t result =
            ::send(socketHandle, payload.data() + sent, payload.size() - sent, MSG_NOSIGNAL);
#endif
        if (result <= 0)
        {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

std::string encodeRecord(
    const sSimulatorDiagnosticRecord& record,
    const std::uint64_t diagnosticDrops)
{
    std::ostringstream output;
    output << "{\"version\":1,\"type\":\"diagnostic\""
           << ",\"loop\":{\"sequence\":" << record.loopSequence
           << ",\"elapsed_ms\":" << record.loopElapsedMs
           << ",\"overrun_ms\":" << record.loopOverrunMs << '}'
           << ",\"control\":{\"state\":\"" << controlStateName(record.control.controlState)
           << "\",\"current_floor\":" << static_cast<unsigned int>(record.control.currentFloor)
           << ",\"target_floor\":" << static_cast<unsigned int>(record.control.targetFloor)
           << ",\"direction\":\"" << directionName(record.control.direction)
           << "\",\"door_open\":" << (record.control.isDoorOpen ? "true" : "false")
           << ",\"faulted\":" << (record.control.isFaulted ? "true" : "false") << '}'
           << ",\"can\":{\"state\":" << static_cast<int>(record.can.state)
           << ",\"fault\":" << static_cast<int>(record.can.faultReason)
           << ",\"progress\":" << record.can.commsProgress
           << ",\"rx\":" << record.can.receivedFrameCount
           << ",\"dropped\":" << record.can.droppedEventCount
           << ",\"tx\":" << record.can.transmittedFrameCount
           << ",\"tx_failed\":" << record.can.transmitFailureCount
           << ",\"hb_expected\":" << static_cast<unsigned int>(record.can.expectedNodeHbReplyMask)
           << ",\"hb_received\":" << static_cast<unsigned int>(record.can.receivedNodeHbReplyMask)
           << ",\"hb_missed\":" << static_cast<unsigned int>(record.can.missedNodeHbReplyMask)
           << ",\"hb_window_open\":" << (record.can.isNodeHbReplyWindowOpen ? "true" : "false") << '}'
           << ",\"database\":{\"state\":" << static_cast<int>(record.databaseState)
           << ",\"fault\":" << static_cast<int>(record.databaseFault)
           << ",\"reads\":" << record.databaseReadCount
           << ",\"dropped\":" << record.databaseDroppedEventCount
           << ",\"writes\":" << record.databaseWriteCount
           << ",\"write_failed\":" << record.databaseWriteFailureCount << '}'
           << ",\"diagnostic_drops\":" << diagnosticDrops << "}\n";
    return output.str();
}

std::string jsonEscape(const char* value)
{
    std::ostringstream output;
    for (const unsigned char character : std::string{value})
    {
        switch (character)
        {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character >= 0x20)
                {
                    output << character;
                }
                break;
        }
    }
    return output.str();
}

std::string encodeTestpoint(const sSimulatorTestpointRecord& record)
{
    std::ostringstream output;
    output << "{\"version\":1,\"type\":\"testpoint\""
           << ",\"sequence\":" << record.sequence
           << ",\"thread_id\":" << record.threadId
           << ",\"name\":\"" << jsonEscape(record.name.data()) << '"'
           << ",\"detail\":\"" << jsonEscape(record.detail.data()) << "\"}\n";
    return output.str();
}

template <std::size_t Size>
void copyText(std::array<char, Size>& destination, const std::string_view source)
{
    const std::size_t length = (std::min)(source.size(), Size - 1);
    std::copy_n(source.data(), length, destination.data());
    destination[length] = '\0';
}

} // namespace

cSimulatorDiagnosticsPublisher::cSimulatorDiagnosticsPublisher() = default;

cSimulatorDiagnosticsPublisher::~cSimulatorDiagnosticsPublisher()
{
    stop();
}

void cSimulatorDiagnosticsPublisher::start()
{
    if (worker_.joinable())
    {
        return;
    }
    host_ = std::getenv("ELEVATOR_SIM_DIAGNOSTICS_HOST");
    const char* portText = std::getenv("ELEVATOR_SIM_DIAGNOSTICS_PORT");
    if (host_ == nullptr || portText == nullptr)
    {
        return;
    }
    const long parsedPort = std::strtol(portText, nullptr, 10);
    if (parsedPort <= 0 || parsedPort > 65535)
    {
        return;
    }
    port_ = static_cast<std::uint16_t>(parsedPort);
    enabled_ = true;
    activePublisher.store(this);
    worker_ = std::jthread([this](const std::stop_token stopToken) {
        run(stopToken);
    });
}

void cSimulatorDiagnosticsPublisher::stop()
{
    cSimulatorDiagnosticsPublisher* expected = this;
    static_cast<void>(activePublisher.compare_exchange_strong(expected, nullptr));
    if (worker_.joinable())
    {
        worker_.request_stop();
        worker_.join();
    }
}

bool cSimulatorDiagnosticsPublisher::tryPublishTestpoint(
    const std::string_view name,
    const std::string_view detail)
{
    if (!enabled_)
    {
        return false;
    }
    std::unique_lock<std::mutex> producerLock{
        testpointProducerMutex_,
        std::try_to_lock};
    if (!producerLock.owns_lock())
    {
        droppedCount_.fetch_add(1);
        return false;
    }

    sSimulatorTestpointRecord record{};
    record.sequence = testpointSequence_.fetch_add(1) + 1;
    record.threadId = static_cast<std::uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    copyText(record.name, name);
    copyText(record.detail, detail);
    if (!testpoints_.tryPush(record))
    {
        droppedCount_.fetch_add(1);
        return false;
    }
    return true;
}

bool cSimulatorDiagnosticsPublisher::tryPublish(const sSimulatorDiagnosticRecord& record)
{
    if (!enabled_)
    {
        return false;
    }
    if (!records_.tryPush(record))
    {
        droppedCount_.fetch_add(1);
        return false;
    }
    return true;
}

std::uint64_t cSimulatorDiagnosticsPublisher::droppedCount() const
{
    return droppedCount_.load();
}

bool cSimulatorDiagnosticsPublisher::enabled() const
{
    return enabled_;
}

void cSimulatorDiagnosticsPublisher::run(const std::stop_token stopToken) noexcept
{
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        return;
    }
#endif
    SocketHandle socketHandle = kInvalidSocket;
    while (!stopToken.stop_requested())
    {
        if (socketHandle == kInvalidSocket)
        {
            socketHandle = connectSocket(host_, port_);
            if (socketHandle == kInvalidSocket)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{250});
                continue;
            }
        }

        sSimulatorTestpointRecord testpoint{};
        if (testpoints_.tryPop(testpoint))
        {
            if (!sendAll(socketHandle, encodeTestpoint(testpoint)))
            {
                closeSocket(socketHandle);
                socketHandle = kInvalidSocket;
                droppedCount_.fetch_add(1);
            }
            continue;
        }

        sSimulatorDiagnosticRecord record{};
        if (!records_.tryPop(record))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            continue;
        }
        if (!sendAll(socketHandle, encodeRecord(record, droppedCount_.load())))
        {
            closeSocket(socketHandle);
            socketHandle = kInvalidSocket;
            droppedCount_.fetch_add(1);
        }
    }
    closeSocket(socketHandle);
#ifdef _WIN32
    WSACleanup();
#endif
}

bool trySimulatorTestpoint(const std::string_view name, const std::string_view detail)
{
    cSimulatorDiagnosticsPublisher* publisher = activePublisher.load();
    return publisher != nullptr && publisher->tryPublishTestpoint(name, detail);
}

sSimulatorDiagnosticRecord makeSimulatorDiagnosticRecord(
    const std::uint64_t loopSequence,
    const std::uint64_t loopElapsedMs,
    const std::uint64_t loopOverrunMs,
    const cSupervisoryApplication& application,
    const sDBMessageExchange& databaseExchange)
{
    return {
        loopSequence,
        loopElapsedMs,
        loopOverrunMs,
        application.snapshot(),
        application.canHealth(),
        databaseExchange.databaseState.load(),
        databaseExchange.faultReason.load(),
        databaseExchange.readCount.load(),
        databaseExchange.droppedEventCount.load(),
        databaseExchange.writeCount.load(),
        databaseExchange.writeFailureCount.load()};
}

} // namespace project6::supervisory
