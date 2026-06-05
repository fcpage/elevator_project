/******************************************************************
* http_server.hpp - HTTP Request Adapter
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Declares the local HTTP listener boundary for the PHP front end.
******************************************************************/

#pragma once

#include <cstdint>
#include <optional>

#include "project6/supervisory/common/event.hpp"
#include "project6/supervisory/common/result.hpp"

namespace project6::supervisory
{

/**
 * @brief Configuration for the local HTTP listener.
 */
struct HttpServerConfig
{
    std::uint16_t port = 8080;
    const char* bindAddress = "127.0.0.1";
};

/**
 * @brief Small local HTTP adapter for web-originated elevator requests.
 *
 * This should parse only the narrow API needed by the PHP front end. Avoid
 * letting web request parsing leak into scheduling or state-machine code.
 */
class HttpServer
{
public:
    explicit HttpServer(HttpServerConfig config);

    OperationStatus initialize();

    /**
     * @brief Attempts to read one pending HTTP request as a supervisor event.
     */
    std::optional<SupervisoryEvent> tryReadEvent();

private:
    HttpServerConfig config_{};
    bool isInitialized_ = false;
};

} // namespace project6::supervisory
