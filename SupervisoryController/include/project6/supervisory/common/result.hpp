/******************************************************************
* result.hpp - Common Operation Status Types
* Author: Project 6 Team
* Last Modified: 2026-05-31
* @brief Declares lightweight status values used across the supervisor.
******************************************************************/

#pragma once

namespace project6::supervisory
{

/**
 * @brief Generic status for operations where detailed error recovery is not
 *        implemented yet.
 */
enum class OperationStatus
{
    Ok,
    NotInitialized,
    InvalidArgument,
    WouldBlock,
    InsufficientPrivileges,
    HardwareUnavailable,
    NetworkUnavailable,
    NotImplemented
};

} // namespace project6::supervisory
