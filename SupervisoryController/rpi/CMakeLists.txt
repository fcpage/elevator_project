cmake_minimum_required(VERSION 3.14)

project(Project6SupervisoryController
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Project6SupervisoryControllerRpi
    VERSION 0.1.0
    LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type." FORCE)
endif()

option(
    SUPERVISORY_ENABLE_AUTO_ARRIVAL
    "Simulate elevator arrival after three seconds for hardware demonstrations."
    OFF
)

option(
    SUPERVISORY_USE_VIRTUAL_CAN
    "Use a preconfigured vcan0 interface for hardware-free integration testing."
    OFF
)

option(
    SUPERVISORY_CAN_INTERFACE_PRECONFIGURED
    "Do not reconfigure CAN from the controller process."
    ON
)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/src/main.cpp")
    set(SUPERVISORY_ROOT "${CMAKE_CURRENT_LIST_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../src/main.cpp")
    get_filename_component(
        SUPERVISORY_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/.."
        ABSOLUTE
    )
else()
    message(FATAL_ERROR
        "Could not locate the supervisory source directory. "
        "Place this CMakeLists.txt in the supervisory project root or its rpi directory."
    )
endif()

find_package(Threads REQUIRED)

add_executable(supervisory_controller
    "${SUPERVISORY_ROOT}/src/main.cpp"
    "${SUPERVISORY_ROOT}/src/app/supervisory_application.cpp"
    "${SUPERVISORY_ROOT}/src/can/can_comms_service.cpp"
    "${SUPERVISORY_ROOT}/src/can/can_protocol.cpp"
    "${SUPERVISORY_ROOT}/src/can/socket_can_adapter.cpp"
    "${SUPERVISORY_ROOT}/src/control/supervisory_state_machine.cpp"
    "${SUPERVISORY_ROOT}/src/drivers/supervisory_drivers.cpp"
    "${SUPERVISORY_ROOT}/src/http/http_server.cpp"
    "${SUPERVISORY_ROOT}/src/scheduler/request_scheduler.cpp"
)

target_include_directories(supervisory_controller
    PRIVATE
        "${SUPERVISORY_ROOT}/include"
)

target_compile_features(supervisory_controller PRIVATE cxx_std_20)

set_target_properties(supervisory_controller PROPERTIES
    CXX_EXTENSIONS OFF
)

target_compile_options(supervisory_controller PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -pthread
)

target_link_libraries(supervisory_controller PRIVATE
        Threads::Threads
        atomic
)

if(SUPERVISORY_ENABLE_AUTO_ARRIVAL)
    target_compile_definitions(supervisory_controller PRIVATE
        SUPERVISORY_ENABLE_AUTO_ARRIVAL=1
    )
endif()

if(SUPERVISORY_USE_VIRTUAL_CAN)
    target_compile_definitions(supervisory_controller PRIVATE
        SUPERVISORY_USE_VIRTUAL_CAN=1
    )
endif()

if(SUPERVISORY_CAN_INTERFACE_PRECONFIGURED)
    target_compile_definitions(supervisory_controller PRIVATE
        SUPERVISORY_CAN_INTERFACE_PRECONFIGURED=1
    )
endif()
