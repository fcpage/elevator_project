/******************************************************************
* can_protocol_tests.cpp - CAN Protocol Tests
* Author: Project 6 Team
* Last Modified: 2026-06-07
* @brief Verifies the shared supervisor command frame layout.
******************************************************************/

#include <array>
#include <concepts>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>

#include "supervisory/can/can_adapter.hpp"
#include "supervisory/can/can_protocol.hpp"

namespace
{

template <typename Adapter>
concept InitializesThroughConstReference = requires(const Adapter& adapter)
{
    adapter.initialize();
};

static_assert(
    !InitializesThroughConstReference<project6::supervisory::cSocketCanAdapter>,
    "SocketCAN initialization mutates adapter state and must not be const");

template <typename Adapter>
concept ReadsFrameWithStatus = requires(
    const Adapter& adapter,
    project6::supervisory::sCanFrame& frame)
{
    { adapter.tryReadFrame(frame) } ->
        std::same_as<project6::supervisory::ecOperationStatus>;
};

static_assert(
    ReadsFrameWithStatus<project6::supervisory::cSocketCanAdapter>,
    "SocketCAN reads must distinguish no data from hardware failure");

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "TEST_FAILURE " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main()
{
    using project6::supervisory::sCanFrame;
    using project6::supervisory::ecCanMessageType;
    using project6::supervisory::decodeNodeHbFrame;
    using project6::supervisory::decodeCanFrame;
    using project6::supervisory::ecDoorCommand;
    using project6::supervisory::ecNodeHb;
    using project6::supervisory::makeNodeHbFrame;
    using project6::supervisory::makeSupervisorArrivalFrame;
    using project6::supervisory::makeSupervisorCommandFrame;
    using project6::supervisory::makeSupervisorDoorFrame;

    constexpr std::array<std::uint8_t, 3> kExpectedPayloads{0x05, 0x06, 0x07};
    constexpr std::array<std::uint8_t, 3> kExpectedArrivalPayloads{0x81, 0x82, 0x83};
    constexpr std::array<std::pair<std::uint8_t, ecNodeHb>, 4> kExpectedNodeHbPayloads{{
        {0x84, ecNodeHb::Ok},
        {0x85, ecNodeHb::SupervisorRequest},
        {0x86, ecNodeHb::NodeRequest},
        {0x87, ecNodeHb::Error},
    }};

    for (std::uint8_t floor = 1; floor <= kExpectedPayloads.size(); ++floor)
    {
        const std::optional<sCanFrame> frame = makeSupervisorCommandFrame(floor, true);

        require(frame.has_value(), "valid floor did not produce a frame");
        require(frame->id == 0x100, "supervisor command used the wrong CAN identifier");
        require(frame->dataLength == 1, "supervisor command used the wrong DLC");
        require(
            frame->data[0] == kExpectedPayloads[floor - 1],
            "supervisor command used the wrong payload");
    }

    require(!makeSupervisorCommandFrame(0, true).has_value(), "floor zero was accepted");
    require(!makeSupervisorCommandFrame(4, true).has_value(), "floor four was accepted");

    for (std::uint8_t floor = 1; floor <= kExpectedArrivalPayloads.size(); ++floor)
    {
        const std::optional<sCanFrame> frame = makeSupervisorArrivalFrame(floor);

        require(frame.has_value(), "valid arrival floor did not produce a frame");
        require(frame->id == 0x100, "arrival frame used the wrong CAN identifier");
        require(frame->dataLength == 1, "arrival frame used the wrong DLC");
        require(
            frame->data[0] == kExpectedArrivalPayloads[floor - 1],
            "arrival frame used the wrong payload");
    }

    require(!makeSupervisorArrivalFrame(0).has_value(), "arrival floor zero was accepted");
    require(!makeSupervisorArrivalFrame(4).has_value(), "arrival floor four was accepted");

    const std::optional<sCanFrame> doorOpenFrame = makeSupervisorDoorFrame(ecDoorCommand::Open);
    require(doorOpenFrame.has_value(), "door-open command was not encoded");
    require(doorOpenFrame->id == 0x100, "door-open command used the wrong CAN identifier");
    require(doorOpenFrame->dataLength == 1, "door-open command used the wrong DLC");
    require(doorOpenFrame->data[0] == 0x88, "door-open command used the wrong payload");

    const std::optional<sCanFrame> doorCloseFrame = makeSupervisorDoorFrame(ecDoorCommand::Close);
    require(doorCloseFrame.has_value(), "door-close command was not encoded");
    require(doorCloseFrame->id == 0x100, "door-close command used the wrong CAN identifier");
    require(doorCloseFrame->dataLength == 1, "door-close command used the wrong DLC");
    require(doorCloseFrame->data[0] == 0x89, "door-close command used the wrong payload");
    require(
        !makeSupervisorDoorFrame(static_cast<ecDoorCommand>(99)).has_value(),
        "unknown door command was encoded");

    for (const auto [payload, expectedType] : kExpectedNodeHbPayloads)
    {
        sCanFrame nodeHbFrame{};
        nodeHbFrame.id = 0x201;
        nodeHbFrame.dataLength = 1;
        nodeHbFrame.data[0] = payload;

        const auto decodedNodeHb = decodeNodeHbFrame(nodeHbFrame);
        require(decodedNodeHb.has_value(), "node heartbeat frame was not decoded");
        require(decodedNodeHb->type == expectedType, "node heartbeat decoded to the wrong type");
        require(decodedNodeHb->sourceId == 0x201, "node heartbeat source ID was not preserved");
        require(decodedNodeHb->payload == payload, "node heartbeat payload was not preserved");
    }

    sCanFrame standardFrame{};
    standardFrame.id = 0x201;
    standardFrame.dataLength = 1;
    standardFrame.data[0] = 0x01;
    require(
        !decodeNodeHbFrame(standardFrame).has_value(),
        "standard shared-protocol frame was decoded as an internal frame");
    const auto decodedStandard = decodeCanFrame(standardFrame);
    require(decodedStandard.has_value(), "standard shared-protocol frame stopped decoding");
    require(
        decodedStandard->type == ecCanMessageType::FloorModuleRequest,
        "standard shared-protocol frame decoded to the wrong type");

    const auto nodeHbOk = makeNodeHbFrame(
        project6::supervisory::kFloorOneControllerCanId,
        ecNodeHb::Ok);
    require(nodeHbOk.has_value(), "valid node heartbeat frame was not encoded");
    require(nodeHbOk->id == 0x201, "node heartbeat frame used the wrong CAN ID");
    require(nodeHbOk->dataLength == 1, "node heartbeat frame used the wrong DLC");
    require(nodeHbOk->data[0] == 0x84, "node heartbeat frame used the wrong payload");

    const auto nodeHbRequest = makeNodeHbFrame(
        project6::supervisory::kSupervisoryControllerCanId,
        ecNodeHb::SupervisorRequest);
    require(nodeHbRequest.has_value(), "valid node heartbeat request was not encoded");
    require(nodeHbRequest->id == 0x100, "node heartbeat request used the wrong CAN ID");
    require(nodeHbRequest->data[0] == 0x85, "node heartbeat request used the wrong payload");

    sCanFrame invalidLength{};
    invalidLength.id = 0x201;
    invalidLength.dataLength = 2;
    invalidLength.data[0] = 0x84;
    require(!decodeNodeHbFrame(invalidLength).has_value(), "invalid node heartbeat DLC was accepted");

    sCanFrame unknownInternal{};
    unknownInternal.id = 0x201;
    unknownInternal.dataLength = 1;
    unknownInternal.data[0] = 0x80;
    require(!decodeNodeHbFrame(unknownInternal).has_value(), "unknown internal payload was accepted");
    require(!decodeCanFrame(unknownInternal).has_value(), "internal payload reached standard decoder");
    require(!makeNodeHbFrame(0x201, ecNodeHb::Unknown).has_value(), "unknown node heartbeat was encoded");

    return 0;
}
