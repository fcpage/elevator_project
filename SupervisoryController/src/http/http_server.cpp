/******************************************************************
* http_server.cpp - HTTP Request Adapter
* Author: Project 6 Team
* Last Modified: 2026-06-04
* @brief Provides the local HTTP listener skeleton for PHP integration.
******************************************************************/

#include "project6/supervisory/http/http_server.hpp"

namespace project6::supervisory
{

HttpServer::HttpServer(HttpServerConfig config)
    : config_(config)
{
}

OperationStatus HttpServer::initialize()
{
    if (config_.bindAddress == nullptr || config_.port == 0)
    {
        return OperationStatus::InvalidArgument;
    }

    isInitialized_ = true;
    return OperationStatus::NotImplemented;
}

std::optional<SupervisoryEvent> HttpServer::tryReadEvent()
{
    if (!isInitialized_)
    {
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace project6::supervisory
