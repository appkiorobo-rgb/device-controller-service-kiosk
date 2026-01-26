cmake_minimum_required(VERSION 3.20)
project(DeviceControllerService VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows Service specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Source files
set(SERVICE_CORE_SOURCES
    src/service_core/service_core.cpp
    src/service_core/device_orchestrator.cpp
    src/service_core/recovery_manager.cpp
)

set(DEVICE_ABSTRACTION_SOURCES
    src/device_abstraction/icamera.cpp
    src/device_abstraction/iprinter.cpp
    src/device_abstraction/ipayment_terminal.cpp
)

set(VENDOR_ADAPTER_SOURCES
    src/vendor_adapters/smartro/serial_port.cpp
    src/vendor_adapters/smartro/smartro_protocol.cpp
    src/vendor_adapters/smartro/smartro_payment_terminal.cpp
)

set(IPC_LAYER_SOURCES
    src/ipc/named_pipe_server.cpp
    src/ipc/command_processor.cpp
)

set(LOGGING_SOURCES
    src/logging/logger.cpp
)

set(COMMON_SOURCES
    src/common/uuid_generator.cpp
)

# Main service executable
add_executable(${PROJECT_NAME}
    src/main/service_main.cpp
    ${SERVICE_CORE_SOURCES}
    ${DEVICE_ABSTRACTION_SOURCES}
    ${VENDOR_ADAPTER_SOURCES}
    ${IPC_LAYER_SOURCES}
    ${LOGGING_SOURCES}
    ${COMMON_SOURCES}
)

# Windows Service linking
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        advapi32  # Windows Service API
        ws2_32    # Named Pipes
    )
endif()

# JSON library (using nlohmann/json)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# Simple status check utility
add_executable(check_status
    check_status.cpp
    ${VENDOR_ADAPTER_SOURCES}
)

if(WIN32)
    target_link_libraries(check_status PRIVATE
        setupapi  # Serial port enumeration
    )
endif()

# Tests
enable_testing()
add_subdirectory(tests)
