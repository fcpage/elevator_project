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

struct sDBInboundSnapshot {
    std::uint32_t index;
    // std::time_t  date; TODO: add these (not sure which methods support this)
    // std::time_t  time;
    std::uint8_t requestedFloor;
    /** guiRequests.remote: 0 normal, 1 maintenance, 3 Sabbath. */
    std::uint8_t remoteMode;
};

struct sDBOutboundSnapshot {
    std::uint32_t index;
    // std::time_t  date; TODO: add these (not sure which methods support this)
    // std::time_t  time;
    std::uint8_t currentFloor;
    bool floorRequest1;
    bool floorRequest2;
    bool floorRequest3;
    bool carRequestFloor1;
    bool carRequestFloor2;
    bool carRequestFloor3;
    bool doors;
};


static inline std::ostream& operator<<(std::ostream& os, const sDBInboundSnapshot& snap) {
    os  << "sDBSnapshot = {\n" 
        << "\trequestedFloor = " << unsigned(snap.requestedFloor) << '\n'
        << "\tindex = " << snap.index << '\n'
        << "};\n";
    return os;
}

static inline std::ostream& operator<<(std::ostream& os, const sDBOutboundSnapshot& snap) {
    os  << "sDBSnapshot = {\n" 
        << "\tcurrentFloor = " << unsigned(snap.currentFloor) << '\n'
        << "\tfloorRequest1 = " << snap.floorRequest1 << '\n'
        << "\tfloorRequest2 = " << snap.floorRequest2 << '\n'
        << "\tfloorRequest3 = " << snap.floorRequest3 << '\n'
        << "\tcarRequestFloor1 = " << snap.carRequestFloor1 << '\n'
        << "\tcarRequestFloor2 = " << snap.carRequestFloor2 << '\n'
        << "\tcarRequestFloor3 = " << snap.carRequestFloor3 << '\n'
        << "\tdoors = " << snap.doors << '\n'
        << "};\n";
    return os;
}

} // namespace project6::supervisory
