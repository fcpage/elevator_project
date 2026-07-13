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
        assert(fail);
        return status_; 
    };
    constexpr V value() { 
        assert(!fail);
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

} // namespace project6::supervisory
