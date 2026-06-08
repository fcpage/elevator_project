/******************************************************************
* http_server.cpp - HTTP Request Adapter
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @brief Provides the local HTTP listener skeleton for PHP integration.
*
* Update 06/06/2026: This stub implementation and HTTP server is not
* required for this project. Comms between the front end and backend
* are handled via a simple SQL database.
*
* Leaving stub implementation as a possible "addon feature" that
* could be implemented if additional time is found.
******************************************************************/

#include "project6/supervisory/http/http_server.hpp"

namespace project6::supervisory
{

HttpServer::HttpServer(HttpServerConfig config)
    : config_(config)
{
}

ecOperationStatus HttpServer::initialize()
{
    if (config_.bindAddress == nullptr || config_.port == 0)
    {
        return ecOperationStatus::InvalidArgument;
    }

    isInitialized_ = true;
    return ecOperationStatus::NotImplemented;
}

std::optional<sSupervisoryEvent> HttpServer::tryReadEvent() const {
    if (!isInitialized_)
    {
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace project6::supervisory
