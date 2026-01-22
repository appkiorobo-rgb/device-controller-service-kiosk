// include/common/uuid_generator.h
#pragma once

#include <string>

namespace device_controller {

// UUIDGenerator - generates UUIDs for commandId and eventId
class UUIDGenerator {
public:
    // Generate a UUID v4 string
    static std::string generate();
};

} // namespace device_controller
