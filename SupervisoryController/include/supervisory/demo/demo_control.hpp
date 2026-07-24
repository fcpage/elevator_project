/******************************************************************
* demo_control.hpp - Phase 2 database-gap workaround
* @brief Reads a small local control file as a temporary DB adapter.
******************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace project6::supervisory
{

class cSupervisoryApplication;

struct sDemoControlConfig
{
    const char* controlFile = "demo_control.txt";
};

/**
 * @brief Converts demo_control.txt values into normal SA adapter events.
 *
 * This is deliberately isolated. It is not a second GUI/backend protocol:
 * when the database branch is ready, its worker should enqueue the same
 * ModeUpdate and MaintenanceFloorRequest events through the application seam.
 *
 * Supported file keys:
 *   mode_bits=0..3
 *   sabbath_stop_ms=1000..3600000
 *   maintenance_request_id=<new token>
 *   maintenance_floor=1..3
 */
class cDemoControl
{
public:
    explicit cDemoControl(sDemoControlConfig config);

    /** Polls the file once; missing files are treated as no demo input. */
    void poll(cSupervisoryApplication& application);

private:
    sDemoControlConfig config_{};
    std::optional<std::uint8_t> lastModeBits_;
    std::string lastMaintenanceRequestId_;
};

} // namespace project6::supervisory
