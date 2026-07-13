/**
 * @file:       database_message_service.hpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#pragma once

#include <cstdint>
namespace project6::supervisory
{

struct sDBTable {
    std::int32_t index;
    // date DATE NOT NULL,
    // time TIME NOT NULL,
    std::int32_t nodeID;
    std::uint8_t sender;
    std::uint8_t receiver;
    std::uint8_t currentFloor;
    std::uint8_t requestFloor;
    std::uint8_t status;
    bool queued;
    bool served;
};


} // namespace project6::supervisory
