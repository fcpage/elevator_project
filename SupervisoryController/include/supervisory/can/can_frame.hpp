/******************************************************************
* can_frame.hpp - CAN Frame Model
* Author: Project 6 Team
* Last Modified: 2026-06-03
* @file can_frame.hpp
* @brief Defines the CAN frame data structure used by the supervisor.
******************************************************************/

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace project6::supervisory
{

//=========== Begin Network Mapping ===========
/**
 * @name Project CAN Node Identifiers
 *
 * Standard 11-bit CAN identifiers assigned by the Project 6 protocol.
 * These constants identify nodes on the physical bus. The protocol codec
 * handles payload interpretation.
 */
///@{
constexpr std::uint16_t kSupervisoryControllerCanId = 0x100;
constexpr std::uint16_t kElevatorControllerCanId = 0x101;
constexpr std::uint16_t kCarControllerCanId = 0x200;
constexpr std::uint16_t kFloorOneControllerCanId = 0x201;
constexpr std::uint16_t kFloorTwoControllerCanId = 0x202;
constexpr std::uint16_t kFloorThreeControllerCanId = 0x203;
///@}

//=========== End of Network Mapping ==========

/**
 * @brief Maximum payload length of a CAN data frame in bytes.
 */
constexpr std::size_t kCanPayloadLength = 8;

//============ End of Frame Constants ===========


/**
 * @brief Standard supervisor controller CAN frame.
 *
 * This type deliberately contains no Linux SocketCAN fields or project-specific
 * payload interpretation. The adapter handles OS translation and the protocol
 * codec validates and interprets the payload.
 */
struct sCanFrame
{
    /**
     * @brief Standard 11-bit CAN identifier stored in a 16-bit integer.
     */
    std::uint16_t id = 0;

    /**
     * @brief Payload storage. Only bytes in the range [0, dataLength) are valid.
     */
    std::array<std::uint8_t, kCanPayloadLength> data{};

    /**
     * @brief Number of valid payload bytes in data.
     */
    std::uint8_t dataLength = 0;
};

} // namespace project6::supervisory
