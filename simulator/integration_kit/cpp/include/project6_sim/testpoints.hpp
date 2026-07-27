/**
 * @file testpoints.hpp
 * @brief Optional breadcrumbs for a Project 6 simulator run.
 *
 * Add this directory to a target's include paths and use
 * PROJECT6_SIM_TESTPOINT("name", "detail").  In an ordinary build the macro
 * compiles to nothing.  In a simulator build it forwards to the bounded,
 * non-blocking diagnostic publisher already linked by project6_supervisory.
 */

#pragma once

#if defined(SUPERVISORY_ENABLE_SIM_TESTPOINTS)
#include "supervisory/sim/simulator_diagnostics.hpp"
#define PROJECT6_SIM_TESTPOINT(name, detail) \
    static_cast<void>(::project6::supervisory::trySimulatorTestpoint((name), (detail)))
#else
#define PROJECT6_SIM_TESTPOINT(name, detail) static_cast<void>(0)
#endif
