// src/ipc/message_parser.cpp
#include "ipc/message_parser.h"
#include "logging/logger.h"
#include <sstream>
#include <regex>
#include <algorithm>

namespace ipc {

// Simple JSON parser (for basic use cases)
// For production, consider using a proper JSON library like nlohmann/json

std::string MessageParser::getJsonString(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"\\s*:\\s*\"([^\"]+)\"";
    std::regex regex(pattern);
    std::smatch match;
    
    if (std::regex_search(json, match, regex)) {
        return match[1].str();
    }
    
    return "";
}

int64_t MessageParser::getJsonInt64(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"\\s*:\\s*(\\d+)";
    std::regex regex(pattern);
    std::smatch match;
    
    if (std::regex_search(json, match, regex)) {
        return std::stoll(match[1].str());
    }
    
    return 0;
}

namespace {
/// JSON 문자열 값에서 이스케이프 해제 (payload 경로 등). \\ -> \ 등.
std::string unescapeJsonStringValue(std::string value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            switch (value[i + 1]) {
                case '\\': out += '\\'; ++i; break;
                case '"':  out += '"';  ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                default:   out += value[i]; break;
            }
        } else {
            out += value[i];
        }
    }
    return out;
}
} // namespace

std::map<std::string, std::string> MessageParser::getJsonObject(const std::string& json, const std::string& key) {
    std::map<std::string, std::string> result;
    
    // Find the object start
    std::string pattern = "\"" + key + "\"\\s*:\\s*\\{";
    std::regex regex(pattern);
    std::smatch match;
    
    if (!std::regex_search(json, match, regex)) {
        return result;
    }
    
    size_t startPos = match.position() + match.length() - 1;
    int braceCount = 1;
    size_t endPos = startPos + 1;
    
    // Find matching closing brace
    while (endPos < json.length() && braceCount > 0) {
        if (json[endPos] == '{') braceCount++;
        else if (json[endPos] == '}') braceCount--;
        endPos++;
    }
    
    if (braceCount != 0) {
        return result;
    }
    
    std::string objStr = json.substr(startPos, endPos - startPos);
    
    // Parse key-value pairs; value may contain JSON escapes (e.g. \\ in Windows path)
    std::regex pairRegex("\"([^\"]+)\"\\s*:\\s*\"([^\"]+)\"");
    std::sregex_iterator iter(objStr.begin(), objStr.end(), pairRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        result[iter->str(1)] = unescapeJsonStringValue(iter->str(2));
    }
    
    return result;
}

std::shared_ptr<Command> MessageParser::parseCommand(const std::string& json) {
    try {
        auto command = std::make_shared<Command>();
        
        command->protocolVersion = getJsonString(json, "protocolVersion");
        command->kind = stringToMessageKind(getJsonString(json, "kind"));
        command->commandId = getJsonString(json, "commandId");
        command->type = stringToCommandType(getJsonString(json, "type"));
        command->timestampMs = getJsonInt64(json, "timestampMs");
        command->payload = getJsonObject(json, "payload");
        
        return command;
    } catch (const std::exception& e) {
        logging::Logger::getInstance().error("Failed to parse command: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<Response> MessageParser::parseResponse(const std::string& json) {
    try {
        auto response = std::make_shared<Response>();
        
        response->protocolVersion = getJsonString(json, "protocolVersion");
        response->kind = stringToMessageKind(getJsonString(json, "kind"));
        response->commandId = getJsonString(json, "commandId");
        response->status = stringToResponseStatus(getJsonString(json, "status"));
        response->timestampMs = getJsonInt64(json, "timestampMs");
        response->responseMap = getJsonObject(json, "result");
        
        // Parse error if present
        std::string errorCode = getJsonString(json, "errorCode");
        if (!errorCode.empty()) {
            auto error = std::make_shared<Error>();
            error->code = errorCode;
            error->message = getJsonString(json, "errorMessage");
            response->error = error;
        }
        
        return response;
    } catch (const std::exception& e) {
        logging::Logger::getInstance().error("Failed to parse response: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<Event> MessageParser::parseEvent(const std::string& json) {
    try {
        auto event = std::make_shared<Event>();
        
        event->protocolVersion = getJsonString(json, "protocolVersion");
        event->kind = stringToMessageKind(getJsonString(json, "kind"));
        event->eventId = getJsonString(json, "eventId");
        event->eventType = stringToEventType(getJsonString(json, "eventType"));
        event->timestampMs = getJsonInt64(json, "timestampMs");
        event->deviceType = getJsonString(json, "deviceType");
        event->data = getJsonObject(json, "data");
        
        return event;
    } catch (const std::exception& e) {
        logging::Logger::getInstance().error("Failed to parse event: " + std::string(e.what()));
        return nullptr;
    }
}

std::string MessageParser::escapeJsonString(const std::string& str) {
    std::ostringstream oss;
    for (unsigned char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (c <= 0x1F || c == 0x7F) {
                    oss << ' ';  // 제어문자·DEL → 공백 (JSON 규격 위반 방지)
                } else {
                    oss << static_cast<char>(c);
                }
                break;
        }
    }
    return oss.str();
}

std::string MessageParser::buildJsonObject(const std::map<std::string, std::string>& obj) {
    if (obj.empty()) {
        return "{}";
    }
    
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& p : obj) {
        if (!first) oss << ",";
        oss << "\"" << escapeJsonString(p.first) << "\":\"" << escapeJsonString(p.second) << "\"";
        first = false;
    }
    oss << "}";
    return oss.str();
}

std::string MessageParser::serializeCommand(const Command& command) {
    std::ostringstream oss;
    oss << "{"
        << "\"protocolVersion\":\"" << command.protocolVersion << "\","
        << "\"kind\":\"" << messageKindToString(command.kind) << "\","
        << "\"commandId\":\"" << command.commandId << "\","
        << "\"type\":\"" << commandTypeToString(command.type) << "\","
        << "\"timestampMs\":" << command.timestampMs << ","
        << "\"payload\":" << buildJsonObject(command.payload)
        << "}";
    return oss.str();
}

std::string MessageParser::serializeResponse(const Response& response) {
    std::ostringstream oss;
    oss << "{"
        << "\"protocolVersion\":\"" << response.protocolVersion << "\","
        << "\"kind\":\"" << messageKindToString(response.kind) << "\","
        << "\"commandId\":\"" << response.commandId << "\","
        << "\"status\":\"" << responseStatusToString(response.status) << "\","
        << "\"timestampMs\":" << response.timestampMs << ",";
    
    if (!response.responseMap.empty()) {
        oss << "\"result\":" << buildJsonObject(response.responseMap) << ",";
    }
    
    if (response.error) {
        oss << "\"errorCode\":\"" << response.error->code << "\","
            << "\"errorMessage\":\"" << escapeJsonString(response.error->message) << "\"";
    } else {
        oss << "\"error\":null";
    }
    
    oss << "}";
    return oss.str();
}

std::string MessageParser::serializeEvent(const Event& event) {
    std::ostringstream oss;
    oss << "{"
        << "\"protocolVersion\":\"" << event.protocolVersion << "\","
        << "\"kind\":\"" << messageKindToString(event.kind) << "\","
        << "\"eventId\":\"" << event.eventId << "\","
        << "\"eventType\":\"" << eventTypeToString(event.eventType) << "\","
        << "\"timestampMs\":" << event.timestampMs << ","
        << "\"deviceType\":\"" << event.deviceType << "\","
        << "\"data\":" << buildJsonObject(event.data)
        << "}";
    return oss.str();
}

} // namespace ipc
