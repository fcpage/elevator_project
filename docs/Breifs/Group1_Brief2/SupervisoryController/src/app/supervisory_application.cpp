/******************************************************************
* supervisory_application.cpp - Supervisory Application Orchestrator
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Provides the top-level polling loop for the controller.
******************************************************************/

#include "project6/supervisory/app/supervisory_application.hpp"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>

#include "project6/supervisory/can/can_protocol.hpp"

namespace project6::supervisory
{

namespace // Logging Helpers for debugging
{

constexpr std::size_t kMaximumCanFramesPerCycle = 16;

const char* canMessageTypeName(const CanMessageType type)
{
    switch (type)
    {
        case CanMessageType::SupervisorCommand:
            return "SupervisorCommand";
        case CanMessageType::ElevatorStatus:
            return "ElevatorStatus";
        case CanMessageType::CarFloorRequest:
            return "CanCarRequest";
        case CanMessageType::FloorModuleRequest:
            return "CanFloorRequest";
    }

    return "Unknown";
}

void logCanFrame(const sCanFrame& frame)
{
    std::clog << "CAN_RX id=0x" << std::hex << std::uppercase << frame.id
              << std::dec << " dlc=" << static_cast<unsigned int>(frame.dataLength)
              << " data=";

    const std::size_t payloadLength =
        std::min<std::size_t>(frame.dataLength, frame.data.size());
    for (std::size_t index = 0; index < payloadLength; ++index)
    {
        std::clog << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned int>(frame.data[index]);
    }

    std::clog << std::dec << std::nouppercase << std::setfill(' ') << '\n';
}

void logDecodedMessage(const sDecodedCanMessage& message)
{
    std::clog << "EVENT type=" << canMessageTypeName(message.type);
    if (message.floor.has_value())
    {
        const char* floorLabel =
            message.type == CanMessageType::ElevatorStatus ? " reported_floor=" : " requested_floor=";
        std::clog << floorLabel << static_cast<unsigned int>(*message.floor);
    }
    std::clog << '\n';
}

} // namespace - Logging Helpers

cSupervisoryApplication::cSupervisoryApplication(cSocketCanAdapter& canAdapter)
    : appCanAdapter_(canAdapter),
      appStateMachine_(canAdapter)
{
}

ecOperationStatus cSupervisoryApplication::initialize()
{
    const ecOperationStatus canStatus = appCanAdapter_.initialize();
    if (canStatus != ecOperationStatus::Ok)
    {
        return canStatus;
    }

    appIsInitialized_ = true;
    return ecOperationStatus::Ok;
}

ecOperationStatus cSupervisoryApplication::runLoopOnce(const std::chrono::milliseconds elapsedMs)
{
    if (!appIsInitialized_)
    {
        return ecOperationStatus::NotInitialized;
    }

    pollCan();
    processTimer(elapsedMs);

    return ecOperationStatus::Ok;
}

void cSupervisoryApplication::pollCan()
{
    for (std::size_t frameCount = 0; frameCount < kMaximumCanFramesPerCycle; ++frameCount)
    {
        const std::optional<sCanFrame> frame = appCanAdapter_.tryReadFrame();
        if (!frame.has_value())
        {
            return;
        }

        logCanFrame(*frame);

        const std::optional<sDecodedCanMessage> message = decodeCanFrame(*frame);
        if (!message.has_value())
        {
            std::clog << "CAN_RX_REJECTED reason=invalid_protocol_frame\n";
            continue;
        }

        logDecodedMessage(*message);

        const std::optional<sSupervisoryEvent> event = toSupervisoryEvent(*message);
        if (!event.has_value())
        {
            std::clog << "CAN_RX_REJECTED reason=no_supervisory_event\n";
            continue;
        }

        appStateMachine_.handleEvent(*event);
    }
}

void cSupervisoryApplication::processTimer(std::chrono::milliseconds elapsedMs)
{
    sSupervisoryEvent saEvent{};
    saEvent.type = ecEventType::TimerTick;
    saEvent.timestampMs = elapsedMs;

    appStateMachine_.handleEvent(saEvent);
}

}
