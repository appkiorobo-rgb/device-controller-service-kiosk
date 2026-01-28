// include/ipc/message_parser.h
#pragma once

#include "ipc/message_types.h"
#include <string>
#include <memory>

namespace ipc {

// Message parser for JSON serialization/deserialization
class MessageParser {
public:
    // Parse command from JSON string
    static std::shared_ptr<Command> parseCommand(const std::string& json);
    
    // Parse response from JSON string
    static std::shared_ptr<Response> parseResponse(const std::string& json);
    
    // Parse event from JSON string
    static std::shared_ptr<Event> parseEvent(const std::string& json);
    
    // Serialize command to JSON string
    static std::string serializeCommand(const Command& command);
    
    // Serialize response to JSON string
    static std::string serializeResponse(const Response& response);
    
    // Serialize event to JSON string
    static std::string serializeEvent(const Event& event);
    
private:
    // Helper functions for JSON parsing
    static std::string getJsonString(const std::string& json, const std::string& key);
    static int64_t getJsonInt64(const std::string& json, const std::string& key);
    static std::map<std::string, std::string> getJsonObject(const std::string& json, const std::string& key);
    
    // Helper functions for JSON building
    static std::string buildJsonObject(const std::map<std::string, std::string>& obj);
    static std::string escapeJsonString(const std::string& str);
};

} // namespace ipc
