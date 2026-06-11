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

#include "project6/supervisory/can/can_adapter.hpp"
#include "project6/supervisory/can/can_protocol.hpp"

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
    using project6::supervisory::makeSupervisorCommandFrame;

    constexpr std::array<std::uint8_t, 3> kExpectedPayloads{0x05, 0x06, 0x07};

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

    return 0;
}
