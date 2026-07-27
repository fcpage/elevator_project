cmake_minimum_required(VERSION 3.14)

project(Project6SupervisoryController
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
# set(CMAKE_EXPORT_COMPILE_COMMANDS)

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

option(
    SUPERVISORY_ENABLE_DEMO_MODES
    "Read demo_control.txt as a temporary database integration seam."
    OFF
)

option(
    SUPERVISORY_ENABLE_MINIAUDIO
    "Enable real miniaudio/ALSA playback."
    OFF
)

set(
    SUPERVISORY_MINIAUDIO_INCLUDE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    CACHE PATH
    "Directory containing miniaudio.h when real audio is enabled."
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

# find_package(PulseAudio REQUIRED)
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
    "${SUPERVISORY_ROOT}/src/audio/announcement_service.cpp"
    "${SUPERVISORY_ROOT}/src/demo/demo_control.cpp"
)

target_include_directories(supervisory_controller
    PRIVATE
        "${SUPERVISORY_ROOT}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Common/"
)

if(SUPERVISORY_ENABLE_MINIAUDIO)
    if(NOT EXISTS "${SUPERVISORY_MINIAUDIO_INCLUDE_DIR}/miniaudio.h")
        message(FATAL_ERROR
            "SUPERVISORY_ENABLE_MINIAUDIO requires miniaudio.h in "
            "SUPERVISORY_MINIAUDIO_INCLUDE_DIR. Set it to the directory "
            "containing miniaudio.h.")
    endif()
    target_include_directories(supervisory_controller PRIVATE ${SUPERVISORY_MINIAUDIO_INCLUDE_DIR})
    target_compile_definitions(supervisory_controller PRIVATE SUPERVISORY_ENABLE_MINIAUDIO=1)
endif()

if(SUPERVISORY_ENABLE_DEMO_MODES)
    target_compile_definitions(supervisory_controller PRIVATE SUPERVISORY_ENABLE_DEMO_MODES=1)
endif()

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
    pulse
    atomic
)

set(SUPERVISORY_AUDIO_DIR "${SUPERVISORY_ROOT}/audio")
if(EXISTS "${SUPERVISORY_AUDIO_DIR}")
    file(GLOB SUPERVISORY_AUDIO_ASSETS CONFIGURE_DEPENDS
        "${SUPERVISORY_AUDIO_DIR}/floor1.wav"
        "${SUPERVISORY_AUDIO_DIR}/floor2.wav"
        "${SUPERVISORY_AUDIO_DIR}/floor3.wav")
    if(SUPERVISORY_AUDIO_ASSETS)
        add_custom_command(TARGET supervisory_controller POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:supervisory_controller>/audio"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SUPERVISORY_AUDIO_ASSETS}
                "$<TARGET_FILE_DIR:supervisory_controller>/audio")
    endif()
endif()

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
