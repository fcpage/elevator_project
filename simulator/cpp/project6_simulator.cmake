# Project 6 simulator consumer helper.
#
# Usage, after project6_supervisory has been linked to TARGET:
#   include("/path/to/simulator/cpp/project6_simulator.cmake")
#   project6_enable_testpoints(TARGET)
#
# The normal simulator launchers already enable this for supervisory_controller.
# This helper is for branch-specific executables that want to add breadcrumbs.

function(project6_enable_testpoints target)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")
    target_compile_definitions(${target} PRIVATE SUPERVISORY_ENABLE_SIM_TESTPOINTS=1)
endfunction()
