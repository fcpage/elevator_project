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

enum class ecDBFloorValues : std::uint8_t {
    None,
    Floor1,
    Floor2,
    Floor3,
};

struct sDBInboundSnapshot {
    std::int32_t index;
    // std::time_t  date; TODO: add these (not sure which methods support this)
    // std::time_t  time;
    ecDBFloorValues requestedFloor;
};

struct sDBOutboundSnapshot {
    std::int32_t index;
    // std::time_t  date; TODO: add these (not sure which methods support this)
    // std::time_t  time;
    ecDBFloorValues currentFloor;
    bool floorRequest1;
    bool floorRequest2;
    bool floorRequest3;
    bool carRequestFloor1;
    bool carRequestFloor2;
    bool carRequestFloor3;
    bool doorsOpen;
};


static inline std::ostream& operator<<(std::ostream& os, const ecDBFloorValues& floor) {
    switch(floor) {
        case ecDBFloorValues::Floor1: {
            os << "Floor1";
            break;
        }
        case ecDBFloorValues::Floor2: {
            os << "Floor2";
            break;
        }
        case ecDBFloorValues::Floor3: {
            os << "Floor3";
            break;
        }
        case ecDBFloorValues::Moving: {
            os << "Moving";
            break;
        }
    }
}

static inline std::ostream& operator<<(std::ostream& os, const sDBInboundSnapshot& snap) {
    os  << "sDBSnapshot = {\n" 
        << "\trequestedFloor = " << snap.requestedFloor << '\n'
        << "\tindex = " << snap.index << '\n'
        << "};\n";
    return os;
}

static inline std::ostream& operator<<(std::ostream& os, const sDBOutboundSnapshot& snap) {
    os  << "sDBSnapshot = {\n" 
        << "\tcurrentFloor = " << snap.currentFloor << '\n'
        << "\tfloorRequest1 = " << snap.floorRequest1 << '\n'
        << "\tfloorRequest2 = " << snap.floorRequest2 << '\n'
        << "\tfloorRequest3 = " << snap.floorRequest3 << '\n'
        << "\tcarRequestFloor1 = " << snap.carRequestFloor1 << '\n'
        << "\tcarRequestFloor2 = " << snap.carRequestFloor2 << '\n'
        << "\tcarRequestFloor3 = " << snap.carRequestFloor3 << '\n'
        << "\tdoorsOpen = " << snap.doorsOpen << '\n'
        << "};\n";
    return os;
}

} // namespace project6::supervisory
