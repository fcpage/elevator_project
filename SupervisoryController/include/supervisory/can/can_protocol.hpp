/******************************************************************
* can_protocol.hpp - Shared CAN Protocol Helpers
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @file can_protocol.hpp
* @brief Converts shared CAN frames into supervisor-level messages.
******************************************************************/

#pragma once

#include <cstdint>
#include <optional>

#include "supervisory/common/event.hpp"
#include "CAN_protocol.h"

namespace project6::supervisory
{
    using namespace CAN;

//=========== Begin Network Mapping ===========
/**
 * @name Project CAN Node Identifiers
 *
 * Standard 11-bit CAN identifiers assigned by the Project 6 protocol.
 * These constants identify nodes on the physical bus. The protocol codec
 * handles payload interpretation.
 */
///@{
constexpr auto kSupervisoryControllerCanId = static_cast<std::uint16_t>(NodeID::NODE_ID_SC);
constexpr auto kElevatorControllerCanId    = static_cast<std::uint16_t>(NodeID::NODE_ID_EC);
constexpr auto kCarControllerCanId         = static_cast<std::uint16_t>(NodeID::NODE_ID_CC);
constexpr auto kFloorOneControllerCanId    = static_cast<std::uint16_t>(NodeID::NODE_ID_FC1);
constexpr auto kFloorTwoControllerCanId    = static_cast<std::uint16_t>(NodeID::NODE_ID_FC2);
constexpr auto kFloorThreeControllerCanId  = static_cast<std::uint16_t>(NodeID::NODE_ID_FC3);
///@}
///
/**
 * @brief Maximum payload length of a CAN data frame in bytes.
 */
constexpr auto kCanPayloadLength = static_cast<std::size_t>(Config::CAN_PAYLOAD_LENGTH);

/**
 * @brief Standard struct of the shared CAN protocol layout.
 *
 * For modularity and adaptability, the supervisory controller does not
 * hard-code payload bit positions outside this codec. Keeping layout details
 * in one structure makes requirement changes localized.
 * I.e.: changing the floor bit field, enable/status bit, or CAN IDs should
 * not require rewrites of the rest of the program. Physical bus timing belongs
 * to SocketCanConfig because Linux applies it to the SocketCAN network device.
 */
struct sCanProtocolConfig
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
    std::uint8_t sharedProtocolDlc = static_cast<std::uint8_t>(Config::DLC);

    /**
     * @brief Lowest valid elevator floor number.
     */
    std::uint8_t minFloor = static_cast<std::uint8_t>(Config::MIN_FLOOR);

    /**
     * @brief Highest valid elevator floor number.
     */
    std::uint8_t maxFloor = static_cast<std::uint8_t>(Config::MAX_FLOOR);

    /**
     * @brief Mask selecting floor bits in byte 0.
     */
    std::uint8_t floorMask = static_cast<std::uint8_t>(Config::FLOOR_MASK);

    /**
     * @brief Right shift applied after floorMask to recover the floor value.
     */
    std::uint8_t floorShift = static_cast<std::uint8_t>(Config::FLOOR_SHIFT);

    /**
     * @brief Bit used as supervisor enable, elevator status, or car metadata.
     */
    std::uint8_t statusOrEnableMask = static_cast<std::uint8_t>(Config::STATUS_OR_ENABLE_MASK);

    /**
     * @brief Bit used by floor controllers to indicate an active request.
     */
    std::uint8_t floorModuleRequestMask = static_cast<std::uint8_t>(Config::FC_REQ_MASK);
};

/**
 * @brief Protocol configuration matching the current project PDF.
 */
inline constexpr sCanProtocolConfig kDefaultCanProtocolConfig{};

constexpr std::uint8_t kSharedProtocolDlc = kDefaultCanProtocolConfig.sharedProtocolDlc;
constexpr std::uint8_t kFloorMask = kDefaultCanProtocolConfig.floorMask;
constexpr std::uint8_t kStatusOrEnableMask = kDefaultCanProtocolConfig.statusOrEnableMask;
constexpr std::uint8_t kFloorModuleRequestMask = kDefaultCanProtocolConfig.floorModuleRequestMask;
/**
 * @brief Byte 0 bit marking project-internal payload extensions.
 *
 * This is not a CAN extended identifier flag. Frames still use standard
 * 11-bit CAN IDs; the protocol extension is detected from the payload MSB.
 */
constexpr std::uint8_t kInternalProtocolFlagMask = static_cast<std::uint8_t>(Messages::EXTENDED_MSG);

/**
 * @brief Node heartbeat payload values after project-internal decode.
 */
enum class ecNodeHb
{
    /** Payload is MSB-set but not a known heartbeat command. */
    Unknown,
    /** Node reports that heartbeat communication is healthy. */
    Ok,
    /** Supervisory controller requests heartbeat replies from all scoped nodes. */
    SupervisorRequest,
    /** A node asks the supervisory controller to prove it can still respond. */
    NodeRequest,
    /** Node reports a heartbeat-related communication error. */
    Error
};

/** Project-internal commands sent by the supervisor to the door controller. */
enum class ecDoorCommand
{
    Open,
    Close
};

enum class ecCanMessageType
{
    /** Outbound command produced by the supervisory controller. */
    SupervisorCommand,

    /** Elevator controller report containing its current floor and status bit. */
    ElevatorStatus,

    /** In-car panel request with the requested floor encoded in byte 0. */
    CarFloorRequest,

    /** Landing request whose floor is inferred from the sender CAN ID. */
    FloorModuleRequest
};

/**
 * @brief Protocol-level message after byte layout decoding.
 *
 * Keep this as the only place outside the codec that talks about CAN IDs,
 * payload bit positions, or DLC expectations from the shared document.
 */
struct sDecodedCanMessage
{
    /** Semantic message category selected from the source CAN ID. */
    ecCanMessageType type = ecCanMessageType::CarFloorRequest;

    /** Original sender identifier retained for diagnostics and validation. */
    std::uint16_t sourceId = 0;

    /**
     * @brief Floor recovered from the payload or floor-controller source ID.
     *
     * Messages that require a floor are rejected by decodeCanFrame() when this
     * value cannot be derived and validated.
     */
    std::optional<std::uint8_t> floor;

    /** Shared protocol flag used as status or enable according to message type. */
    bool statusBit = false;
};

/**
 * @brief Project-internal payload extension decoded from byte 0.
 */
struct sNodeHbMessage
{
    /** Node heartbeat message type selected from the full byte 0 value. */
    ecNodeHb type = ecNodeHb::Unknown;

    /** Standard CAN ID of the node that transmitted the frame. */
    std::uint16_t sourceId = 0;

    /** Raw byte 0 payload retained for diagnostics and future extension. */
    std::uint8_t payload = 0;
};

/**
 * @brief Decodes one raw CAN frame using the supplied protocol configuration.
 *
 * Validation is fail-closed: an unexpected DLC, unknown source ID, invalid
 * floor field, or inactive floor-module request returns std::nullopt.
 *
 * @param frame Raw CAN frame from hardware, simulator, or tests.
 * @param config Protocol layout and node IDs to apply.
 * @return Decoded message when the frame is valid for the protocol; otherwise
 *         std::nullopt.
 */
std::optional<sDecodedCanMessage> decodeCanFrame(
    const sCanFrame& frame,
    const sCanProtocolConfig& config = kDefaultCanProtocolConfig);

/**
 * @brief Decodes project-internal payload extension frames.
 *
 * Only known node heartbeat payloads are accepted. Unknown MSB-set payloads
 * return std::nullopt so future internal message types cannot silently enter
 * the heartbeat path.
 *
 * @param frame Raw CAN frame from hardware, simulator, or tests.
 * @param config Protocol layout and DLC expectation to apply.
 * @return Decoded node heartbeat message, or std::nullopt for non-heartbeat
 *         frames, invalid DLC, or unknown internal payloads.
 */
std::optional<sNodeHbMessage> decodeNodeHbFrame(
    const sCanFrame& frame,
    const sCanProtocolConfig& config = kDefaultCanProtocolConfig);

/**
 * @brief Converts a decoded protocol message into a state-machine event.
 *
 * Supervisor command frames are intentionally not converted because they are
 * outbound messages produced by the supervisor.
 *
 * @param message Validated protocol message produced by decodeCanFrame().
 * @return A normalized event for inbound messages; otherwise std::nullopt.
 */
std::optional<sSupervisoryEvent> toSupervisoryEvent(const sDecodedCanMessage& message);

/**
 * @brief Builds the one-byte command sent from the supervisor to the elevator.
 *
 * Byte 0 is constructed by masking the shifted floor field and OR-ing the
 * optional enable bit. Remaining payload bytes stay zero-initialized.
 *
 * @param targetFloor Requested destination floor.
 * @param enable Whether the supervisor enable bit should be set.
 * @param config Protocol layout and node IDs to apply.
 * @return Encoded frame when the floor is valid; otherwise std::nullopt.
 */
std::optional<sCanFrame> makeSupervisorCommandFrame(
    std::uint8_t targetFloor,
    bool enable,
    const sCanProtocolConfig& config = kDefaultCanProtocolConfig);

/**
 * @brief Builds a serviced-floor notification for hall-light clearing.
 *
 * @param servicedFloor Floor whose hall request has been completed.
 * @param config Protocol layout and node IDs to apply.
 * @return Encoded frame when the floor is valid; otherwise std::nullopt.
 */
std::optional<sCanFrame> makeSupervisorArrivalFrame(
    std::uint8_t servicedFloor,
    const sCanProtocolConfig& config = kDefaultCanProtocolConfig);

/**
 * @brief Builds a project-internal door command from the supervisor.
 *
 * @param command Requested door action.
 * @param config Protocol layout and node IDs to apply.
 * @return Encoded door command, or std::nullopt for an unknown command.
 */
std::optional<sCanFrame> makeSupervisorDoorFrame(
    ecDoorCommand command,
    const sCanProtocolConfig& config = kDefaultCanProtocolConfig);

/**
 * @brief Builds a one-byte heartbeat frame for the project-internal protocol.
 *
 * @param sourceId Standard 11-bit CAN ID to put on the outgoing frame.
 * @param type Heartbeat payload to encode into byte 0.
 * @param config Protocol layout and DLC expectation to apply.
 * @return Encoded heartbeat frame, or std::nullopt when type is Unknown.
 */
std::optional<sCanFrame> makeNodeHbFrame(
    std::uint16_t sourceId,
    ecNodeHb type,
    const sCanProtocolConfig& config = kDefaultCanProtocolConfig);

} // namespace project6::supervisory
