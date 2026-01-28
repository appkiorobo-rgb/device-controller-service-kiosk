cmake_minimum_required(VERSION 3.15)

project(DeviceControllerService VERSION 1.0.0 LANGUAGES CXX)

# =========================
# 기본 설정
# =========================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 64-bit 강제
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Building for 64-bit")
else()
    message(FATAL_ERROR "This project requires 64-bit build")
endif()

# Build type 기본값
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# =========================
# 공통 include (프로젝트 헤더)
# =========================
set(PROJECT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# =========================
# Dependencies
# =========================
# No external dependencies required (HTTP/WebSocket removed)

# =========================
# 소스 파일
# =========================

# Logger is header-only
set(LOGGING_SOURCES
)

set(SMARTRO_SOURCES
    src/vendor_adapters/smartro/serial_port.cpp
    src/vendor_adapters/smartro/smartro_protocol.cpp
    src/vendor_adapters/smartro/smartro_comm.cpp
)

set(IPC_SOURCES
    src/ipc/named_pipe_server.cpp
    src/ipc/ipc_server.cpp
    src/ipc/message_parser.cpp
)

set(CORE_SOURCES
    src/core/device_manager.cpp
    src/core/service_core.cpp
)

set(VENDOR_ADAPTER_SOURCES
    src/vendor_adapters/smartro/smartro_payment_adapter.cpp
)

# =========================
# Executables
# =========================

add_executable(device_controller_service
    src/main.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
    ${IPC_SOURCES}
    ${CORE_SOURCES}
    ${VENDOR_ADAPTER_SOURCES}
)

add_executable(test_integrated
    tests/test_integrated.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
)

add_executable(test_pipe_client
    tests/test_pipe_client.cpp
)

# =========================
# Target include / link
# =========================

# 프로젝트 include는 target 단위로 지정 (전역 include_directories 지양)
target_include_directories(device_controller_service PRIVATE
    ${PROJECT_INCLUDE_DIR}
)

target_include_directories(test_integrated PRIVATE
    ${PROJECT_INCLUDE_DIR}
)

target_include_directories(test_pipe_client PRIVATE
    ${PROJECT_INCLUDE_DIR}
)

# No external libraries required

# =========================
# Windows 설정
# =========================
if(WIN32)
    # Windows API 레벨 고정 (Windows 10 기준)
    target_compile_definitions(device_controller_service PRIVATE _WIN32_WINNT=0x0A00)
    target_compile_definitions(test_integrated          PRIVATE _WIN32_WINNT=0x0A00)
    target_compile_definitions(test_pipe_client        PRIVATE _WIN32_WINNT=0x0A00)
endif()

# =========================
# Visual Studio 디버그 인자
# =========================
set_target_properties(test_integrated PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS "COM3 DEFAULT_TERM 115200"
)

# =========================
# 컴파일러 옵션
# =========================
if(MSVC)
    target_compile_options(device_controller_service PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    target_compile_options(test_integrated PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    target_compile_options(test_pipe_client PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
else()
        target_compile_options(device_controller_service PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(test_integrated          PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(test_pipe_client         PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =========================
# Debug info
# =========================
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(device_controller_service PRIVATE /Zi)
        target_compile_options(test_integrated          PRIVATE /Zi)
    else()
        target_compile_options(device_controller_service PRIVATE -g)
        target_compile_options(test_integrated          PRIVATE -g)
    endif()
endif()

# =========================
# Install
# =========================
install(TARGETS device_controller_service test_integrated test_pipe_client
    RUNTIME DESTINATION bin
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
