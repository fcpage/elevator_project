#include <iostream>
#include <queue>
#include <regex>
#include <string>

namespace
{
constexpr int kSupervisorCanId = 0x100;
constexpr int kElevatorCanId = 0x101;
constexpr int kCarCanId = 0x200;
constexpr int kFloor1CanId = 0x201;
constexpr int kFloor2CanId = 0x202;
constexpr int kFloor3CanId = 0x203;
constexpr int kFloorMask = 0x03;
constexpr int kStatusOrEnableMask = 0x04;

std::queue<int> pendingFloors;
bool busy = false;

int extractInt(const std::string& line, const std::string& key, int fallback = 0)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    if (!std::regex_search(line, match, pattern))
    {
        return fallback;
    }
    return std::stoi(match[1].str());
}

int extractFirstDataByte(const std::string& line, int fallback = 0)
{
    const std::regex pattern("\"data\"\\s*:\\s*\\[\\s*(-?\\d+)");
    std::smatch match;
    if (!std::regex_search(line, match, pattern))
    {
        return fallback;
    }
    return std::stoi(match[1].str());
}

void sendCanCommand(int floor)
{
    const int payload = kStatusOrEnableMask | (floor & kFloorMask);
    std::cout << "{\"type\":\"can_tx\",\"id\":" << kSupervisorCanId
              << ",\"data\":[" << payload << "]}" << std::endl;
}

void sendIdleLog()
{
    std::cout << "{\"type\":\"log\",\"message\":\"idle\"}" << std::endl;
}

void dispatchNext()
{
    if (busy || pendingFloors.empty())
    {
        sendIdleLog();
        return;
    }

    const int floor = pendingFloors.front();
    pendingFloors.pop();
    busy = true;
    sendCanCommand(floor);
}

bool isRequestSource(int canId)
{
    return canId == kCarCanId || canId == kFloor1CanId || canId == kFloor2CanId || canId == kFloor3CanId;
}

int floorFromFloorControllerId(int canId)
{
    if (canId == kFloor1CanId)
    {
        return 1;
    }
    if (canId == kFloor2CanId)
    {
        return 2;
    }
    if (canId == kFloor3CanId)
    {
        return 3;
    }
    return 0;
}
}

int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.find("\"type\":\"web_request\"") != std::string::npos)
        {
            pendingFloors.push(extractInt(line, "floor"));
            dispatchNext();
            continue;
        }

        if (line.find("\"type\":\"can_rx\"") != std::string::npos)
        {
            const int canId = extractInt(line, "id");
            if (isRequestSource(canId))
            {
                if (canId == kCarCanId)
                {
                    pendingFloors.push(extractFirstDataByte(line) & kFloorMask);
                }
                else
                {
                    pendingFloors.push(floorFromFloorControllerId(canId));
                }
                dispatchNext();
                continue;
            }

            if (canId == kElevatorCanId && (extractFirstDataByte(line) & kStatusOrEnableMask) == 0)
            {
                busy = false;
                dispatchNext();
                continue;
            }
        }

        sendIdleLog();
    }

    return 0;
}
