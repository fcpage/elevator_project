# WINDOWS CMakeLists.txt DO NOT USE TO BUILD Linux
# This script is used to build the executable on Windows,
# it purposely leaves out features that aren't necessary for
# Windows testing.

cmake_minimum_required(VERSION 3.14)

project(Project6SupervisoryController
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    SUPERVISORY_ENABLE_AUTO_ARRIVAL
    "Simulate elevator arrival after three seconds for hardware demonstrations."
    OFF
)

option(
    SUPERVISORY_ENABLE_DEMO_MODES
    "Read demo_control.txt as a temporary database integration seam."
    OFF
)

option(
    SUPERVISORY_ENABLE_MINIAUDIO
    "Enable real miniaudio/ALSA playback; otherwise log demo announcements."
    OFF
)

set(
    SUPERVISORY_MINIAUDIO_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    CACHE PATH
    "Directory containing miniaudio.h when real audio is enabled."
)

find_package(Threads REQUIRED)

set(SUPERVISORY_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Common")

add_library(project6_supervisory
    src/app/supervisory_application.cpp
    src/can/can_comms_service.cpp
    src/can/can_protocol.cpp
    src/can/socket_can_adapter.cpp
    src/control/supervisory_state_machine.cpp
    src/drivers/supervisory_drivers.cpp
    src/audio/announcement_service.cpp
    src/demo/demo_control.cpp
    src/http/http_server.cpp
    src/scheduler/request_scheduler.cpp
)

target_include_directories(project6_supervisory
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "${SUPERVISORY_COMMON_DIR}"
)

if(SUPERVISORY_ENABLE_MINIAUDIO)
    if(NOT EXISTS "${SUPERVISORY_MINIAUDIO_INCLUDE_DIR}/miniaudio.h")
        message(FATAL_ERROR
            "SUPERVISORY_ENABLE_MINIAUDIO requires miniaudio.h in "
            "SUPERVISORY_MINIAUDIO_INCLUDE_DIR. Set it to the directory "
            "containing miniaudio.h.")
    endif()
    target_include_directories(project6_supervisory PRIVATE ${SUPERVISORY_MINIAUDIO_INCLUDE_DIR})
    target_compile_definitions(project6_supervisory PRIVATE SUPERVISORY_ENABLE_MINIAUDIO=1)
endif()

if(SUPERVISORY_ENABLE_DEMO_MODES)
    target_compile_definitions(project6_supervisory PRIVATE SUPERVISORY_ENABLE_DEMO_MODES=1)
endif()

target_link_libraries(project6_supervisory
    PUBLIC
        Threads::Threads
)

if(SUPERVISORY_ENABLE_AUTO_ARRIVAL)
    target_compile_definitions(project6_supervisory
        PRIVATE
            SUPERVISORY_ENABLE_AUTO_ARRIVAL=1
    )
endif()

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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/sim/simulator_supervisor_main.cpp")
    add_executable(supervisory_simulator_adapter
        src/sim/simulator_supervisor_main.cpp
    )

    target_link_libraries(supervisory_simulator_adapter
        PRIVATE
            project6_supervisory
    )
endif()

enable_testing()

add_executable(supervisory_state_machine_smoke
    tests/state_machine_smoke.cpp
    src/can/can_protocol.cpp
    src/control/supervisory_state_machine.cpp
    src/scheduler/request_scheduler.cpp
)

target_include_directories(supervisory_state_machine_smoke
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "${SUPERVISORY_COMMON_DIR}"
)

target_compile_definitions(supervisory_state_machine_smoke
    PRIVATE
        SUPERVISORY_ENABLE_AUTO_ARRIVAL=1
        SUPERVISORY_TEST_EXPECT_AUTO_ARRIVAL=1
)

add_executable(supervisory_state_machine_production_smoke
    tests/state_machine_smoke.cpp
    src/can/can_protocol.cpp
    src/control/supervisory_state_machine.cpp
    src/scheduler/request_scheduler.cpp
)

target_include_directories(supervisory_state_machine_production_smoke
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "${SUPERVISORY_COMMON_DIR}"
)

add_executable(supervisory_can_protocol_tests
    tests/can_protocol_tests.cpp
    src/can/can_protocol.cpp
    src/can/socket_can_adapter.cpp
)

target_include_directories(supervisory_can_protocol_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "${SUPERVISORY_COMMON_DIR}"
)

add_executable(supervisory_spsc_queue_tests
    tests/spsc_queue_tests.cpp
)

target_include_directories(supervisory_spsc_queue_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(supervisory_can_comms_service_tests
    tests/can_comms_service_tests.cpp
    src/can/can_comms_service.cpp
    src/can/can_protocol.cpp
    src/can/socket_can_adapter.cpp
)

target_include_directories(supervisory_can_comms_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "${SUPERVISORY_COMMON_DIR}"
)

target_link_libraries(supervisory_can_comms_service_tests
    PRIVATE
        Threads::Threads
)

add_executable(supervisory_control_loop_tests
    tests/control_loop_tests.cpp
    src/app/supervisory_application.cpp
    src/audio/announcement_service.cpp
    src/can/can_comms_service.cpp
    src/can/can_protocol.cpp
    src/can/socket_can_adapter.cpp
    src/control/supervisory_state_machine.cpp
    src/scheduler/request_scheduler.cpp
)

target_include_directories(supervisory_control_loop_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "${SUPERVISORY_COMMON_DIR}"
)

target_link_libraries(supervisory_control_loop_tests
    PRIVATE
        Threads::Threads
)

add_executable(supervisory_modes_audio_tests
    tests/modes_audio_tests.cpp
    src/app/supervisory_application.cpp
    src/audio/announcement_service.cpp
    src/can/can_comms_service.cpp
    src/control/supervisory_state_machine.cpp
    src/drivers/supervisory_drivers.cpp
    src/can/can_protocol.cpp
    src/can/socket_can_adapter.cpp
    src/scheduler/request_scheduler.cpp
)

target_include_directories(supervisory_modes_audio_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "${SUPERVISORY_COMMON_DIR}"
)

add_test(
    NAME supervisory_state_machine_smoke
    COMMAND supervisory_state_machine_smoke
)

add_test(
    NAME supervisory_can_protocol_tests
    COMMAND supervisory_can_protocol_tests
)

add_test(
    NAME supervisory_state_machine_production_smoke
    COMMAND supervisory_state_machine_production_smoke
)

add_test(
    NAME supervisory_spsc_queue_tests
    COMMAND supervisory_spsc_queue_tests
)

add_test(
    NAME supervisory_can_comms_service_tests
    COMMAND supervisory_can_comms_service_tests
)

add_test(
    NAME supervisory_control_loop_tests
    COMMAND supervisory_control_loop_tests
)

add_test(
    NAME supervisory_modes_audio_tests
    COMMAND supervisory_modes_audio_tests
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
