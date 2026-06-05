/******************************************************************
* can_protocol.hpp - Shared CAN Protocol Helpers
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @brief Converts shared CAN frames into supervisor-level messages.
******************************************************************/

#pragma once

#include <cstdint>
#include <optional>

#include "project6/supervisory/can/can_frame.hpp"
#include "project6/supervisory/common/event.hpp"

namespace project6::supervisory
{

/**
 * @brief Runtime-independent description of the shared CAN protocol layout.
 *
 * For modularity and adaptability, the supervisory controller should not
 * hard-code payload bit positions outside this codec. Keeping layout details
 * in one structure makes requirement changes localized.
 * I.e.: changing the floor bit field, enable/status bit, or CAN IDs should
 * not require rewrites of the rest of the program. Physical bus timing belongs
 * to SocketCanConfig because Linux applies it to the SocketCAN network device.
 */
struct CanProtocolConfig
{
    /**
     * @brief CAN ID used by the supervisory controller when transmitting.
     */
    std::uint16_t supervisoryControllerCanId = kSupervisoryControllerCanId;

    /**
     * @brief CAN ID used by the elevator controller.
     */
    std::uint16_t elevatorControllerCanId = kElevatorControllerCanId;

    /**
     * @brief CAN ID used by the car controller.
     */
    std::uint16_t carControllerCanId = kCarControllerCanId;

    /**
     * @brief CAN ID used by the floor 1 controller.
     */
    std::uint16_t floorOneControllerCanId = kFloorOneControllerCanId;

    /**
     * @brief CAN ID used by the floor 2 controller.
     */
    std::uint16_t floorTwoControllerCanId = kFloorTwoControllerCanId;

    /**
     * @brief CAN ID used by the floor 3 controller.
     */
    std::uint16_t floorThreeControllerCanId = kFloorThreeControllerCanId;

    /**
     * @brief Expected data length code for shared protocol frames.
     */
    std::uint8_t sharedProtocolDlc = 1;

    /**
     * @brief Lowest valid elevator floor number.
     */
    std::uint8_t minFloor = 1;

    /**
     * @brief Highest valid elevator floor number.
     */
    std::uint8_t maxFloor = 3;

    /**
     * @brief Mask selecting floor bits in byte 0.
     */
    std::uint8_t floorMask = 0x03;

    /**
     * @brief Right shift applied after floorMask to recover the floor value.
     */
    std::uint8_t floorShift = 0;

    /**
     * @brief Bit used as supervisor enable, elevator status, or car metadata.
     */
    std::uint8_t statusOrEnableMask = 0x04;

    /**
     * @brief Bit used by floor controllers to indicate an active request.
     */
    std::uint8_t floorModuleRequestMask = 0x01;
};

/**
 * @brief Protocol configuration matching the current project PDF.
 */
inline constexpr CanProtocolConfig kDefaultCanProtocolConfig{};

constexpr std::uint8_t kSharedProtocolDlc = kDefaultCanProtocolConfig.sharedProtocolDlc;
constexpr std::uint8_t kFloorMask = kDefaultCanProtocolConfig.floorMask;
constexpr std::uint8_t kStatusOrEnableMask = kDefaultCanProtocolConfig.statusOrEnableMask;
constexpr std::uint8_t kFloorModuleRequestMask = kDefaultCanProtocolConfig.floorModuleRequestMask;

enum class CanMessageType
{
    SupervisorCommand,
    ElevatorStatus,
    CarFloorRequest,
    FloorModuleRequest
};

/**
 * @brief Protocol-level message after byte layout decoding.
 *
 * Keep this as the only place outside the codec that talks about CAN IDs,
 * payload bit positions, or DLC expectations from the shared document.
 */
struct DecodedCanMessage
{
    CanMessageType type = CanMessageType::CarFloorRequest;
    std::uint16_t sourceId = 0;
    std::optional<std::uint8_t> floor;
    bool statusBit = false;
};

/**
 * @brief Decodes one raw CAN frame using the supplied protocol configuration.
 *
 * @param frame Raw CAN frame from hardware, simulator, or tests.
 * @param config Protocol layout and node IDs to apply.
 * @return Decoded message when the frame is valid for the protocol; otherwise
 *         std::nullopt.
 */
std::optional<DecodedCanMessage> decodeCanFrame(
    const CanFrame& frame,
    const CanProtocolConfig& config = kDefaultCanProtocolConfig);

/**
 * @brief Converts a decoded protocol message into a state-machine event.
 *
 * Supervisor command frames are intentionally not converted because they are
 * outbound messages produced by the supervisor.
 */
std::optional<SupervisoryEvent> toSupervisoryEvent(const DecodedCanMessage& message);

/**
 * @brief Builds the one-byte command sent from the supervisor to the elevator.
 *
 * @param targetFloor Requested destination floor.
 * @param enable Whether the supervisor enable bit should be set.
 * @param config Protocol layout and node IDs to apply.
 * @return Encoded frame when the floor is valid; otherwise std::nullopt.
 */
std::optional<CanFrame> makeSupervisorCommandFrame(
    std::uint8_t targetFloor,
    bool enable,
    const CanProtocolConfig& config = kDefaultCanProtocolConfig);

} // namespace project6::supervisory
