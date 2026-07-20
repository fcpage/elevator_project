/**
 * @file:       database_message_service.hpp
 * @author:     Nigel Sinclair
 * @brief:      MySQL database service abstraction
 */

#pragma once

#include <cstdint>
#include <chrono>

namespace project6::supervisory
{

struct sDBSnapshot {
    std::int32_t index;
    std::time_t  date;
    std::time_t  time;
    std::int32_t nodeID;
    std::uint8_t sender;
    std::uint8_t receiver;
    std::uint8_t currentFloor;
    std::uint8_t requestFloor;
    std::uint8_t status;
    bool queued;
    bool served;
};

static inline std::ostream& operator<<(std::ostream& os, const sDBSnapshot& snap) {
    os  << "sDBSnapshot = {\n" 
        << "\tindex = " << snap.index
        << "\tdate = " << snap.date
        << "\ttime = " << snap.time
        << "\tnodeID = " << snap.nodeID
        << "\tsender = " << snap.sender
        << "\treceiver = " << snap.receiver
        << "\tcurrentFloor = " << snap.currentFloor
        << "\trequestFloor = " << snap.requestFloor
        << "\tstatus = " << snap.status
        << "\tqueued = " << snap.queued
        << "};\n";
    return os;
}


} // namespace project6::supervisory
