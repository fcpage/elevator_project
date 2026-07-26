/******************************************************************
* runtime_can_service.hpp - Build-selected CAN service
* @brief Keeps the production entry point independent of simulator transport.
******************************************************************/

#pragma once

#ifdef SUPERVISORY_USE_SIMULATOR_CAN
#include "supervisory/sim/simulator_can_service.hpp"
#else
#include "supervisory/can/can_comms_service.hpp"
#endif

namespace project6::supervisory
{

#ifdef SUPERVISORY_USE_SIMULATOR_CAN
using cRuntimeCanService = cSimulatorCanService;
#else
using cRuntimeCanService = cCanCommsService;
#endif

} // namespace project6::supervisory
