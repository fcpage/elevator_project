/**
********************************************************************************
* @file     : CAN_Protocol.h
* @brief    : Defines the common CAN protocol
* By        : Nigel Sinclair
********************************************************************************
*/
#ifndef __CAN_PROTOCOL_H
#define __CAN_PROTOCOL_H

#ifdef __cplusplus
#include <cstdint>
#include <array>

namespace CAN {
#define CAN_NAMESPACE(name, type) class name : std::type
#else
#define CAN_NAMESPACE(name) CAN_##name : type
#include <stdint.h>
#endif

//=========== Begin Network Mapping ===========
/**
 * @name Project CAN Node Identifiers
 *
 * Standard 11-bit CAN identifiers assigned by the Project 6 protocol.
 * These constants identify nodes on the physical bus. The protocol codec
 * handles payload interpretation.
 */
enum CAN_NAMESPACE(NodeID, uint16_t) {
    ///@{
    NODE_ID_SC  = 0x0100,		// Supervisory Controller
    NODE_ID_EC  = 0x0101,       // Elevator controller
    NODE_ID_CC  = 0x0200,		// Car Controller
    NODE_ID_FC1 = 0x0201,		// Floor 1 controller
    NODE_ID_FC2 = 0x0202,		// Floor 2 controller
    NODE_ID_FC3 = 0x0203,		// Floor 3 controller
    ///@}
};

//=========== End of Network Mapping ==========

/** 
 * @brief: Helper Masks for CAN bit manipulation
 */
enum CAN_NAMESPACE(Config, uint16_t) {
    DLC                   = 0x01,     // Data length code   
    MIN_FLOOR             = 1,
    MAX_FLOOR             = 3,
    FLOOR_MASK            = 0x03,     // Accesses floor bits
    FLOOR_SHIFT           = 0,
    STATUS_OR_ENABLE_MASK = 0x04,
    FC_REQ_MASK           = 0x01,
    CAN_PAYLOAD_LENGTH    = 8,
};

#ifdef __cplusplus
#define CAN_FLOOR_BITS(n) ( ( (n) << (std::uint8_t)CAN::Config::FLOOR_SHIFT ) & (std::uint8_t)CAN::Config::FLOOR_MASK )
#else
#define CAN_FLOOR_BITS(n) ( ( (n) << FLOOR_SHIFT ) & FLOOR_MASK )
#endif 

/**
 * @brief:  CAN elevator messages *SOME VARIANTS HAVE SAME VALUE*
 */
enum CAN_NAMESPACE(Messages, uint8_t) {
    SC_ENABLE       = 0b0100,               // SC can enable/disable the elevator
    FC_FLOOR_REQ    = 0b0001,               // FC requests the elevator from SC
    SC_REQ_FLOOR_1  = CAN_FLOOR_BITS(1),    // SC requests EC sends elevator to Fn
    SC_REQ_FLOOR_2  = CAN_FLOOR_BITS(2),    // "
    SC_REQ_FLOOR_3  = CAN_FLOOR_BITS(3),    // "
    EC_STATUS       = 0b0100,               // EC reports its state back to the SC
    EC_POS_MOVING   = 0b0000,               // "
    EC_POS_FLOOR_1  = CAN_FLOOR_BITS(1),    // "
    EC_POS_FLOOR_2  = CAN_FLOOR_BITS(2),    // "
    EC_POS_FLOOR_3  = CAN_FLOOR_BITS(3),    // "
    CC_REQ_FLOOR_1  = CAN_FLOOR_BITS(1),    // "
    CC_REQ_FLOOR_2  = CAN_FLOOR_BITS(2),    // "
    CC_REQ_FLOOR_3  = CAN_FLOOR_BITS(3),    // "

    /*** Extended messages (not in common protocol) ***/
#ifndef CAN_COMMON
    EXTENDED_MSG    = 0b10000000,    // Indicates that the message is not in the common protocol
    HB_FLAG         = EXTENDED_MSG  | 0b100,    // Indicates the message is a heartbeat message
    HB_OK           = HB_FLAG       | 0,        // Each node sends this message to report successful operation
    HB_SC_REQ       = HB_FLAG       | 1,        // Supervisory controller request for heartbeat message
    HB_NODE_REQ     = HB_FLAG       | 2,        // Node request heartbeat from SC
    HB_ERR          = HB_FLAG       | 3,        // Each node sends this message to report error to SC

    /* Supervisory controller messages to indicate which floor the elevator has arrived at */
    SC_POS_FLOOR_1  = CAN_FLOOR_BITS(1) | EXTENDED_MSG,
    SC_POS_FLOOR_2  = CAN_FLOOR_BITS(2) | EXTENDED_MSG,
    SC_POS_FLOOR_3  = CAN_FLOOR_BITS(3) | EXTENDED_MSG,
#endif
};

struct sCanFrame
{
#ifdef __cplusplus
    /**
     * @brief Standard 11-bit CAN identifier stored in a 16-bit integer.
     */
    std::uint16_t id = 0;

    /**
     * @brief Payload storage. Only bytes in the range [0, dataLength) are valid.
     */
    std::array<std::uint8_t, static_cast<std::size_t>(Config::CAN_PAYLOAD_LENGTH)> data{};

    /**
     * @brief Number of valid payload bytes in data.
     */
    std::uint8_t dataLength = 0;
#else
    /**
     * @brief Standard 11-bit CAN identifier stored in a 16-bit integer.
     */
    uint16_t id; // default 0

    /**
     * @brief Payload storage. Only bytes in the range [0, dataLength) are valid.
     */
    uint8_t data[CAN_PAYLOAD_LENGTH];

    /**
     * @brief Number of valid payload bytes in data.
     */
    uint8_t dataLength; // defaults 0
#endif
};


#ifdef __cplusplus
}
#endif

#undef CAN_NAMESPACE
#undef CAN_FLOOR_BITS
#endif
