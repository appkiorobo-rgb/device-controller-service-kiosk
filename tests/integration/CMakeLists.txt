# Integration test client executable
add_executable(test_payment_client
    test_payment_client.cpp
)

# Link Windows libraries
target_link_libraries(test_payment_client PRIVATE
    ws2_32  # Named Pipes
)

# JSON library (using nlohmann/json from parent)
# Parent CMakeLists.txt already fetches nlohmann/json, so we just link to it
target_link_libraries(test_payment_client PRIVATE nlohmann_json::nlohmann_json)

# Set C++ standard
set_target_properties(test_payment_client PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
