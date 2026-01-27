cmake_minimum_required(VERSION 3.15)
project(DeviceControllerService VERSION 1.0.0 LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 64비트 빌드 강제
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Building for 64-bit")
else()
    message(FATAL_ERROR "This project requires 64-bit build")
endif()

# 빌드 타입 설정
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 출력 디렉토리 설정
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include 디렉토리
include_directories(${CMAKE_SOURCE_DIR}/include)

# 소스 파일들
set(LOGGING_SOURCES
    src/logging/logger.cpp
)

set(SMARTRO_SOURCES
    src/vendor_adapters/smartro/serial_port.cpp
    src/vendor_adapters/smartro/smartro_protocol.cpp
    src/vendor_adapters/smartro/smartro_comm.cpp
)

# 테스트 프로그램
add_executable(test_device_check
    tests/test_device_check.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
)

add_executable(test_payment_wait
    tests/test_payment_wait.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
)

add_executable(test_card_uid_read
    tests/test_card_uid_read.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
)

add_executable(test_event_wait
    tests/test_event_wait.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
)

add_executable(test_integrated
    tests/test_integrated.cpp
    ${LOGGING_SOURCES}
    ${SMARTRO_SOURCES}
)

# Windows 관련 링크 라이브러리
target_link_libraries(test_device_check
    PRIVATE
)

target_link_libraries(test_payment_wait
    PRIVATE
)

target_link_libraries(test_card_uid_read
    PRIVATE
)

target_link_libraries(test_event_wait
    PRIVATE
)

target_link_libraries(test_integrated
    PRIVATE
)

# Visual Studio 디버그 인자 설정 (예: COM3)
# 실제 사용할 COM 포트로 변경하세요
set_target_properties(test_device_check PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS "COM3 DEFAULT_TERM 115200"
)

set_target_properties(test_payment_wait PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS "COM3 DEFAULT_TERM 115200"
)

set_target_properties(test_card_uid_read PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS "COM3 DEFAULT_TERM 115200"
)

set_target_properties(test_event_wait PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS "COM3 DEFAULT_TERM 115200 0"
)

set_target_properties(test_integrated PROPERTIES
    VS_DEBUGGER_COMMAND_ARGUMENTS "COM3 DEFAULT_TERM 115200"
)

# 컴파일러 플래그
if(MSVC)
    # MSVC 특정 설정
    target_compile_options(test_device_check PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    
    target_compile_options(test_payment_wait PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    
    target_compile_options(test_card_uid_read PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    
    target_compile_options(test_event_wait PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    
    target_compile_options(test_integrated PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    
    # 64비트 명시적 설정
    set_target_properties(test_device_check PROPERTIES
        VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE x64
    )
    
    set_target_properties(test_payment_wait PROPERTIES
        VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE x64
    )
    
    set_target_properties(test_card_uid_read PROPERTIES
        VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE x64
    )
    
    set_target_properties(test_event_wait PROPERTIES
        VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE x64
    )
    
    set_target_properties(test_integrated PROPERTIES
        VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE x64
    )
else()
    # GCC/Clang 설정
    target_compile_options(test_device_check PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    
    target_compile_options(test_payment_wait PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    
    target_compile_options(test_card_uid_read PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    
    target_compile_options(test_event_wait PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    
    target_compile_options(test_integrated PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# 디버그 정보 포함
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(test_device_check PRIVATE /Zi)
        target_compile_options(test_payment_wait PRIVATE /Zi)
        target_compile_options(test_card_uid_read PRIVATE /Zi)
        target_compile_options(test_event_wait PRIVATE /Zi)
        target_compile_options(test_integrated PRIVATE /Zi)
    else()
        target_compile_options(test_device_check PRIVATE -g)
        target_compile_options(test_payment_wait PRIVATE -g)
        target_compile_options(test_card_uid_read PRIVATE -g)
        target_compile_options(test_event_wait PRIVATE -g)
        target_compile_options(test_integrated PRIVATE -g)
    endif()
endif()

# 설치 설정 (선택사항)
install(TARGETS test_device_check test_payment_wait test_card_uid_read test_event_wait test_integrated
    RUNTIME DESTINATION bin
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
