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

# EDSDK paths (adjust if needed)
set(EDSDK_ROOT ${CMAKE_SOURCE_DIR}/edsdk)

# Actual folder structure: Dll, Library (capital letters)
# Try multiple possible folder structures
# Option 1: Standard EDSDK SDK structure (Header folder)
set(EDSDK_INCLUDE_DIR ${EDSDK_ROOT}/Header)
set(EDSDK_LIB_DIR ${EDSDK_ROOT}/Library)
set(EDSDK_BIN_DIR ${EDSDK_ROOT}/Dll)

# Option 1b: Alternative structure (include folder)
if(NOT EXISTS ${EDSDK_INCLUDE_DIR})
    set(EDSDK_INCLUDE_DIR ${EDSDK_ROOT}/include)
endif()

# Option 2: Standard structure (lib/bin subdirectories)
if(NOT EXISTS ${EDSDK_LIB_DIR})
    set(EDSDK_LIB_DIR ${EDSDK_ROOT}/lib)
endif()
if(NOT EXISTS ${EDSDK_BIN_DIR})
    set(EDSDK_BIN_DIR ${EDSDK_ROOT}/bin)
endif()

# Option 3: If files are directly in edsdk root
if(NOT EXISTS ${EDSDK_INCLUDE_DIR})
    set(EDSDK_INCLUDE_DIR ${EDSDK_ROOT})
endif()
if(NOT EXISTS ${EDSDK_LIB_DIR})
    set(EDSDK_LIB_DIR ${EDSDK_ROOT})
endif()
if(NOT EXISTS ${EDSDK_BIN_DIR})
    set(EDSDK_BIN_DIR ${EDSDK_ROOT})
endif()

# Option 4: Check relative path from sample code location (../../../EDSDK/Header/)
# Sample code uses: ./../../../EDSDK/Header/
# From CameraControl folder: ../../../EDSDK/Header/ = C:/Users/kioro/Desktop/VC/EDSDK/Header/
set(EDSDK_SAMPLE_PATH "C:/Users/kioro/Desktop/VC/EDSDK/Header")
if(EXISTS ${EDSDK_SAMPLE_PATH} AND EXISTS "${EDSDK_SAMPLE_PATH}/EDSDK.h")
    set(EDSDK_INCLUDE_DIR ${EDSDK_SAMPLE_PATH})
    message(STATUS "Using EDSDK header from sample code location: ${EDSDK_INCLUDE_DIR}")
endif()

# Check if EDSDK directories exist
if(EXISTS ${EDSDK_INCLUDE_DIR})
    # Check if header files exist
    if(EXISTS "${EDSDK_INCLUDE_DIR}/EDSDK.h")
        message(STATUS "EDSDK include directory found: ${EDSDK_INCLUDE_DIR}")
        message(STATUS "EDSDK.h found")
    else()
        message(WARNING "EDSDK include directory found but EDSDK.h not found in ${EDSDK_INCLUDE_DIR}")
        message(WARNING "Please ensure EDSDK.h is available")
    endif()
else()
    message(WARNING "EDSDK include directory not found: ${EDSDK_INCLUDE_DIR}")
    message(WARNING "Please ensure EDSDK SDK is installed and EDSDK.h is available")
    message(WARNING "Expected locations:")
    message(WARNING "  - ${EDSDK_ROOT}/Header/EDSDK.h")
    message(WARNING "  - ${EDSDK_ROOT}/include/EDSDK.h")
    message(WARNING "  - ${EDSDK_ROOT}/EDSDK.h")
endif()

# Check for library directory
if(EXISTS ${EDSDK_LIB_DIR})
    message(STATUS "EDSDK library directory found: ${EDSDK_LIB_DIR}")
else()
    message(WARNING "EDSDK library directory not found: ${EDSDK_LIB_DIR}")
endif()

# Check for DLL directory
if(EXISTS ${EDSDK_BIN_DIR})
    message(STATUS "EDSDK DLL directory found: ${EDSDK_BIN_DIR}")
else()
    message(WARNING "EDSDK DLL directory not found: ${EDSDK_BIN_DIR}")
endif()

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
    src/core/service_core_detect_hardware.cpp
)

set(VENDOR_ADAPTER_SOURCES
    src/vendor_adapters/smartro/smartro_payment_adapter.cpp
    src/vendor_adapters/windows/windows_gdi_printer_adapter.cpp
)

set(LV77_SOURCES
    src/vendor_adapters/lv77/lv77_comm.cpp
    src/vendor_adapters/lv77/lv77_bill_adapter.cpp
)

set(CANON_SOURCES
    src/vendor_adapters/canon/edsdk_camera_adapter.cpp
    src/vendor_adapters/canon/edsdk_command_processor.cpp
    src/vendor_adapters/canon/edsdk_commands.cpp
    src/vendor_adapters/canon/edsdk_event_handler.cpp
    src/vendor_adapters/canon/edsdk_camera_model.cpp
    src/vendor_adapters/canon/edsdk_liveview_server.cpp
)

set(CONFIG_SOURCES
    src/config/config_manager.cpp
)

# =========================
# Executables
# =========================
# 배포 빌드(x64-Release-Static)에서는 device_controller_service만 빌드
if(BUILD_FOR_DISTRIBUTION)
    set(BUILD_TESTS OFF)
else()
    set(BUILD_TESTS ON)
endif()

add_executable(device_controller_service
    src/main.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
    ${IPC_SOURCES}
    ${CORE_SOURCES}
    ${VENDOR_ADAPTER_SOURCES}
    ${CANON_SOURCES}
    ${CONFIG_SOURCES}
)

if(BUILD_TESTS)
add_executable(test_integrated
    tests/test_integrated.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
)

add_executable(test_pipe_client
    tests/test_pipe_client.cpp
)

add_executable(test_camera_client
    tests/test_camera_client.cpp
)

# LV77 bill validator test client (standalone exe, 9600 8E1)
add_executable(test_lv77_client
    tests/test_lv77_client.cpp
    src/vendor_adapters/smartro/serial_port.cpp
    src/vendor_adapters/lv77/lv77_comm.cpp
)

# EDSDK-only camera test (no IPC) - like test_integrated but for camera
add_executable(test_edsdk_camera
    tests/test_edsdk_camera.cpp
    ${LOGGING_SOURCES}
    ${CONFIG_SOURCES}
    ${CANON_SOURCES}
)
endif()

# =========================
# Target include / link
# =========================

# 프로젝트 include는 target 단위로 지정 (전역 include_directories 지양)
# EDSDK include directory - only add if it exists and contains EDSDK.h
if(EXISTS ${EDSDK_INCLUDE_DIR} AND EXISTS "${EDSDK_INCLUDE_DIR}/EDSDK.h")
    target_include_directories(device_controller_service PRIVATE
        ${PROJECT_INCLUDE_DIR}
        ${EDSDK_INCLUDE_DIR}
    )
    message(STATUS "EDSDK include directory added: ${EDSDK_INCLUDE_DIR}")
else()
    target_include_directories(device_controller_service PRIVATE
        ${PROJECT_INCLUDE_DIR}
    )
    message(WARNING "EDSDK include directory not added - EDSDK.h not found")
    message(WARNING "Please install EDSDK SDK and ensure EDSDK.h is available")
endif()

if(BUILD_TESTS)
target_include_directories(test_integrated PRIVATE
    ${PROJECT_INCLUDE_DIR}
)

target_include_directories(test_pipe_client PRIVATE
    ${PROJECT_INCLUDE_DIR}
)

target_include_directories(test_camera_client PRIVATE
    ${PROJECT_INCLUDE_DIR}
)

target_include_directories(test_lv77_client PRIVATE
    ${PROJECT_INCLUDE_DIR}
)

if(EXISTS ${EDSDK_INCLUDE_DIR} AND EXISTS "${EDSDK_INCLUDE_DIR}/EDSDK.h")
    target_include_directories(test_edsdk_camera PRIVATE
        ${PROJECT_INCLUDE_DIR}
        ${EDSDK_INCLUDE_DIR}
    )
else()
    target_include_directories(test_edsdk_camera PRIVATE
        ${PROJECT_INCLUDE_DIR}
    )
endif()
endif()

# EDSDK library linking
# IMPORTANT: For 64-bit builds, we need 64-bit libraries
# Sample code uses: EDSDK_64/Library/ for x64 builds
# Find EDSDK.lib (search in Library directory, lib directory, or root)
# Priority: Library folder (should contain 64-bit lib if available)
file(GLOB EDSDK_LIB_FILES 
    "${EDSDK_ROOT}/Library/*.lib"
    "${EDSDK_LIB_DIR}/*.lib"
    "${EDSDK_ROOT}/*.lib"
)

if(EDSDK_LIB_FILES)
    target_link_libraries(device_controller_service PRIVATE ${EDSDK_LIB_FILES})
    if(BUILD_TESTS)
        target_link_libraries(test_edsdk_camera PRIVATE ${EDSDK_LIB_FILES})
    endif()
    message(STATUS "EDSDK libraries linked: ${EDSDK_LIB_FILES}")
    message(STATUS "NOTE: Ensure these are 64-bit libraries for x64 build")
else()
    message(WARNING "EDSDK library files (*.lib) not found in ${EDSDK_LIB_DIR} or ${EDSDK_ROOT}")
    message(WARNING "Please ensure EDSDK.lib (64-bit version) is available for linking")
    message(WARNING "For 64-bit builds, you need 64-bit EDSDK libraries")
endif()

# Copy DLL to output directory (search in Dll directory, bin directory, or root)
# IMPORTANT: For 64-bit builds, we need 64-bit DLL
file(GLOB EDSDK_DLL_FILES 
    "${EDSDK_ROOT}/Dll/*.dll"
    "${EDSDK_BIN_DIR}/*.dll"
    "${EDSDK_ROOT}/*.dll"
)

if(EDSDK_DLL_FILES)
    add_custom_command(TARGET device_controller_service POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${EDSDK_DLL_FILES}
        $<TARGET_FILE_DIR:device_controller_service>
        COMMENT "Copying EDSDK DLL files"
    )
    if(BUILD_TESTS)
        add_custom_command(TARGET test_edsdk_camera POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${EDSDK_DLL_FILES}
            $<TARGET_FILE_DIR:test_edsdk_camera>
            COMMENT "Copying EDSDK DLL files for test_edsdk_camera"
        )
    endif()
    message(STATUS "EDSDK DLL files will be copied: ${EDSDK_DLL_FILES}")
    message(STATUS "NOTE: Ensure these are 64-bit DLLs for x64 build")
else()
    message(WARNING "EDSDK DLL files (*.dll) not found in ${EDSDK_BIN_DIR} or ${EDSDK_ROOT}")
    message(WARNING "Please ensure EDSDK.dll (64-bit version) is available at runtime")
    message(WARNING "For 64-bit builds, you need 64-bit EDSDK DLLs")
endif()

# No other external libraries required

# =========================
# Windows 설정
# =========================
if(WIN32)
    target_link_libraries(device_controller_service PRIVATE ws2_32 gdiplus)
    target_compile_definitions(device_controller_service PRIVATE _WIN32_WINNT=0x0A00)
    if(BUILD_TESTS)
        target_compile_definitions(test_integrated     PRIVATE _WIN32_WINNT=0x0A00)
        target_compile_definitions(test_pipe_client   PRIVATE _WIN32_WINNT=0x0A00)
        target_compile_definitions(test_camera_client PRIVATE _WIN32_WINNT=0x0A00)
        target_compile_definitions(test_lv77_client    PRIVATE _WIN32_WINNT=0x0A00)
        target_compile_definitions(test_edsdk_camera   PRIVATE _WIN32_WINNT=0x0A00)
    endif()
endif()

# =========================
# Visual Studio 디버그 인자
# =========================
if(BUILD_TESTS)
set_target_properties(test_integrated PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS "COM3 DEFAULT_TERM 115200"
)
endif()

# =========================
# 컴파일러 옵션 (C++17 필수: structured binding 등 사용)
# =========================
if(MSVC)
    target_compile_options(device_controller_service PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
        /std:c++17
    )
    if(BUILD_TESTS)
        target_compile_options(test_integrated   PRIVATE /W4 /permissive- /Zc:__cplusplus /std:c++17)
        target_compile_options(test_pipe_client PRIVATE /W4 /permissive- /Zc:__cplusplus /std:c++17)
        target_compile_options(test_camera_client PRIVATE /W4 /permissive- /Zc:__cplusplus /std:c++17)
        target_compile_options(test_edsdk_camera PRIVATE /W4 /permissive- /Zc:__cplusplus /std:c++17)
        target_compile_options(test_lv77_client  PRIVATE /W4 /permissive- /Zc:__cplusplus /std:c++17)
    endif()
else()
    target_compile_options(device_controller_service PRIVATE -Wall -Wextra -Wpedantic)
    if(BUILD_TESTS)
        target_compile_options(test_integrated   PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(test_pipe_client  PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(test_camera_client PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(test_edsdk_camera PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(test_lv77_client  PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# =========================
# Debug info
# =========================
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(device_controller_service PRIVATE /Zi)
        if(BUILD_TESTS)
            target_compile_options(test_integrated PRIVATE /Zi)
        endif()
    else()
        target_compile_options(device_controller_service PRIVATE -g)
        if(BUILD_TESTS)
            target_compile_options(test_integrated PRIVATE -g)
        endif()
    endif()
endif()

# =========================
# Install
# =========================
if(BUILD_TESTS)
install(TARGETS device_controller_service test_integrated test_pipe_client test_camera_client test_lv77_client test_edsdk_camera
    RUNTIME DESTINATION bin
)
else()
install(TARGETS device_controller_service RUNTIME DESTINATION bin)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
