cmake_minimum_required(VERSION 3.20)

project(Project6SupervisoryController
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(project6_supervisory
    src/app/supervisory_application.cpp
    src/can/can_protocol.cpp
    src/can/socket_can_adapter.cpp
    src/control/supervisory_state_machine.cpp
    src/drivers/supervisory_drivers.cpp
    src/http/http_server.cpp
    src/scheduler/request_scheduler.cpp
)

target_include_directories(project6_supervisory
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(project6_supervisory PRIVATE /W4)
else()
    target_compile_options(project6_supervisory PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(supervisory_controller
    src/main.cpp
)

target_link_libraries(supervisory_controller
    PRIVATE
        project6_supervisory
)

add_executable(supervisory_simulator_adapter
    src/sim/simulator_supervisor_main.cpp
)

target_link_libraries(supervisory_simulator_adapter
    PRIVATE
        project6_supervisory
)

add_executable(supervisory_state_machine_smoke
    tests/state_machine_smoke.cpp
)

target_link_libraries(supervisory_state_machine_smoke
    PRIVATE
        project6_supervisory
)

add_executable(supervisory_can_protocol_tests
    tests/can_protocol_tests.cpp
)

target_link_libraries(supervisory_can_protocol_tests
    PRIVATE
        project6_supervisory
)

set(ESEPP_EXECUTABLE
    "${CMAKE_CURRENT_SOURCE_DIR}/ESE_Preprocessor/executable/esepp.exe"
    CACHE FILEPATH "Path to the ESE preprocessor executable."
)

add_custom_target(generate_ese
    COMMAND "${ESEPP_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/src"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Regenerating ESE state machine code in SupervisoryController/src."
)
