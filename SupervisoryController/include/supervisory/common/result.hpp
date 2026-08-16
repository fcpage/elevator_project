/******************************************************************
* result.hpp - Common Operation Status Types
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Declares lightweight status values used across the supervisor.
******************************************************************/

#pragma once

#include <cassert>
namespace project6::supervisory
{

/**
 * @brief Generic status for operations where detailed error recovery is not
 *        implemented yet.
 */
enum class ecOperationStatus
{
    Ok,
    NotInitialized,
    InvalidArgument,
    WouldBlock,
    InsufficientPrivileges,
    HardwareUnavailable,
    NetworkUnavailable,
    NotImplemented,
    DatabaseException
};

/** @brief Wraps funciton return in struct containing the status or 
 *         a return value */
template<typename S, typename V>
struct sChoice {
    constexpr bool err() { 
        return fail_; 
    };
    constexpr S status() { 
        assert(fail_);
        return status_; 
    };
    constexpr V value() { 
        assert(!fail_);
        return value_;
    };
    sChoice(S s) : fail_(true), status_(s) {}
    sChoice(V v) : fail_(false), value_(v) {}
private:
    bool fail_;
    union {
        S status_;
        V value_;
    };
};

constexpr const char* operationStatusMessage(const project6::supervisory::ecOperationStatus status)
{
    using project6::supervisory::ecOperationStatus;

    switch (status)
    {
        case ecOperationStatus::Ok:
            return "operation completed successfully";
        case ecOperationStatus::NotInitialized:
            return "a required module was not initialized";
        case ecOperationStatus::InvalidArgument:
            return "invalid runtime configuration";
        case ecOperationStatus::WouldBlock:
            return "operation would block";
        case ecOperationStatus::InsufficientPrivileges:
            return "permission denied while configuring CAN; run as root or grant CAP_NET_ADMIN";
        case ecOperationStatus::HardwareUnavailable:
            return "required hardware or SocketCAN interface is unavailable";
        case ecOperationStatus::NetworkUnavailable:
            return "required network service is unavailable";
        case ecOperationStatus::NotImplemented:
            return "requested operation is not implemented";
        case ecOperationStatus::DatabaseException:
            return "database exception thrown";
    }

    return "unknown operation status";
}


} // namespace project6::supervisory
