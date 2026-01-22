cmake_minimum_required(VERSION 3.20)

# Test framework (using Google Test)
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Test executables
add_subdirectory(unit)
