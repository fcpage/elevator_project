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
namespace CAN {
#define CAN_NAMESPACE(name) name
#else
#define CAN_NAMESPACE(name) CAN_##name
#endif

/**
 * @brief:  All valid values for CAN Node ID in system
 */
enum CAN_NAMESPACE(NodeID) {
    NODE_ID_SC  = 0x0100,		// Supervisory Controller
    NODE_ID_EC  = 0x0101,       // Elevator controller
    NODE_ID_CC  = 0x0200,		// Car Controller
    NODE_ID_FC1 = 0x0201,		// Floor 1 controller
    NODE_ID_FC2 = 0x0202,		// Floor 2 controller
    NODE_ID_FC3 = 0x0203,		// Floor 3 controller
};

/**
 * @brief:  CAN elevator messages *SOME VARIANTS HAVE SAME VALUE*
 */
enum CAN_NAMESPACE(Messages) {
    SC_ENABLE       = 0b0100, // SC can enable/disable the elevator
    FC_FLOOR_REQ    = 0b0001, // FC requests the elevator from SC
    SC_REQ_FLOOR_1  = 0b0001, // SC requests EC sends elevator to Fn
    SC_REQ_FLOOR_2  = 0b0010, // "
    SC_REQ_FLOOR_3  = 0b0011, // "
    EC_STATUS       = 0b0100, // EC reports its state back to the SC
    EC_POS_MOVING   = 0b0000, // "
    EC_POS_FLOOR_1  = 0b0001, // "
    EC_POS_FLOOR_2  = 0b0010, // "
    EC_POS_FLOOR_3  = 0b0011, // "
    CC_REQ_FLOOR_1  = 0b0001, // "
    CC_REQ_FLOOR_2  = 0b0010, // "
    CC_REQ_FLOOR_3  = 0b0011, // "
};

#ifdef __cplusplus
}
#endif

#undef CAN_NAMESPACE
#endif
