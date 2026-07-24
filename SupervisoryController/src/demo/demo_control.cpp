/******************************************************************
* demo_control.cpp - Phase 2 database-gap workaround
******************************************************************/

#include "supervisory/demo/demo_control.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "supervisory/app/supervisory_application.hpp"
#include "supervisory/common/event.hpp"

namespace project6::supervisory
{

namespace
{

std::string trim(std::string value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::unordered_map<std::string, std::string> readValues(const char* path)
{
    std::unordered_map<std::string, std::string> values;
    if (path == nullptr)
    {
        return values;
    }

    std::ifstream input(path);
    if (!input.is_open())
    {
        return values;
    }

    std::string line;
    while (std::getline(input, line))
    {
        line = trim(line);
        if (line.empty() || line.front() == '#')
        {
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
        {
            continue;
        }
        values[trim(line.substr(0, separator))] = trim(line.substr(separator + 1));
    }

    return values;
}

std::optional<unsigned long> parseUnsigned(
    const std::unordered_map<std::string, std::string>& values,
    const std::string& key)
{
    const auto iterator = values.find(key);
    if (iterator == values.end())
    {
        return std::nullopt;
    }

    try
    {
        std::size_t consumed = 0;
        const unsigned long value = std::stoul(iterator->second, &consumed, 0);
        if (consumed != iterator->second.size())
        {
            return std::nullopt;
        }
        return value;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

} // namespace

cDemoControl::cDemoControl(sDemoControlConfig config)
    : config_(config)
{
}

void cDemoControl::poll(cSupervisoryApplication& application)
{
    const auto values = readValues(config_.controlFile);

    if (const auto modeBits = parseUnsigned(values, "mode_bits");
        modeBits.has_value() && *modeBits <= 3 &&
        (!lastModeBits_.has_value() || *lastModeBits_ != *modeBits))
    {
        sSupervisoryEvent event{};
        event.type = ecEventType::ModeUpdate;
        event.modeBits = static_cast<std::uint8_t>(*modeBits);
        if (!application.enqueueAdapterEvent(event))
        {
            std::cerr << "DEMO_CONTROL_QUEUE_FULL key=mode_bits\n";
        }
        lastModeBits_ = static_cast<std::uint8_t>(*modeBits);
    }

    if (const auto stopMs = parseUnsigned(values, "sabbath_stop_ms");
        stopMs.has_value() && *stopMs >= 1000 && *stopMs <= 3600000)
    {
        application.setSabbathStopDuration(
            std::chrono::milliseconds{static_cast<std::int64_t>(*stopMs)});
    }

    const auto requestId = values.find("maintenance_request_id");
    const auto floor = parseUnsigned(values, "maintenance_floor");
    if (requestId != values.end() && floor.has_value() &&
        *floor >= 1 && *floor <= 3 && requestId->second != lastMaintenanceRequestId_)
    {
        sSupervisoryEvent event{};
        event.type = ecEventType::MaintenanceFloorRequest;
        event.requestedFloor = static_cast<std::uint8_t>(*floor);
        if (!application.enqueueAdapterEvent(event))
        {
            std::cerr << "DEMO_CONTROL_QUEUE_FULL key=maintenance_request_id\n";
        }
        else
        {
            lastMaintenanceRequestId_ = requestId->second;
        }
    }
}

} // namespace project6::supervisory
