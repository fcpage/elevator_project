# Copy the relevant blocks into the branch's existing CMakeLists.txt.
#
# Placement: after the project options and before target definitions for the
# options; add target_sources after project6_supervisory is declared; add the
# compile definitions after supervisory_controller is declared.
#
# This does not create an executable or a second main(). It only makes the
# existing application select the localhost CAN bridge when requested.

option(SUPERVISORY_BUILD_SIMULATOR
    "Build the normal supervisory_controller with localhost simulator CAN." OFF)
option(SUPERVISORY_ENABLE_SIM_DIAGNOSTICS
    "Enable best-effort simulator snapshots over localhost." OFF)
option(SUPERVISORY_ENABLE_SIM_TESTPOINTS
    "Enable optional non-blocking PROJECT6_SIM_TESTPOINT calls." OFF)

# Placement: append to the branch's existing project6_supervisory target.
# This is the complete minimum required for plant-only operation.
target_sources(project6_supervisory PRIVATE
    src/sim/simulator_can_service.cpp)

# Placement: after the branch's normal project6_supervisory link libraries.
# The localhost CAN bridge uses Winsock only on Windows.
if(WIN32)
    target_link_libraries(project6_supervisory PUBLIC ws2_32)
endif()

# OPTIONAL: Add this only on a branch that has the baseline database exchange.
# simulator_diagnostics captures database health and testpoints; it is not a
# dependency of the CAN plant.
# target_sources(project6_supervisory PRIVATE
#     src/sim/simulator_diagnostics.cpp)

# Placement: after the branch's normal supervisory_controller target exists.
# Add simulator_diagnostics.cpp above before enabling this option.
if(SUPERVISORY_BUILD_SIMULATOR)
    target_compile_definitions(supervisory_controller
        PRIVATE SUPERVISORY_USE_SIMULATOR_CAN=1)
endif()

if(SUPERVISORY_ENABLE_SIM_DIAGNOSTICS)
    target_compile_definitions(supervisory_controller
        PRIVATE SUPERVISORY_ENABLE_SIM_DIAGNOSTICS=1)
endif()

if(SUPERVISORY_ENABLE_SIM_TESTPOINTS)
    target_compile_definitions(project6_supervisory
        PRIVATE SUPERVISORY_ENABLE_SIM_TESTPOINTS=1)
    target_compile_definitions(supervisory_controller
        PRIVATE SUPERVISORY_ENABLE_SIM_TESTPOINTS=1)
endif()
