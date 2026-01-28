// include/ipc/message_types.h
#pragma once

#include <string>
#include <map>
#include <memory>
#include <cstdint>

namespace ipc {

// Protocol version
constexpr const char* PROTOCOL_VERSION = "1.0";

// Message kind
enum class MessageKind {
    COMMAND,
    RESPONSE,
    EVENT
};

// Response status
enum class ResponseStatus {
    OK,
    FAILED,
    REJECTED
};

// Command types
enum class CommandType {
    PAYMENT_START,
    PAYMENT_CANCEL,
    PAYMENT_TRANSACTION_CANCEL,
    PAYMENT_STATUS,
    PAYMENT_RESET,
    PAYMENT_DEVICE_CHECK,
    PAYMENT_CARD_UID_READ,
    PAYMENT_LAST_APPROVAL,
    PAYMENT_IC_CARD_CHECK,
    PAYMENT_SCREEN_SOUND_SETTING,
    GET_DEVICE_LIST,
    GET_STATE_SNAPSHOT
};

// Event types
enum class EventType {
    PAYMENT_COMPLETE,
    PAYMENT_FAILED,
    PAYMENT_CANCELLED,
    DEVICE_STATE_CHANGED,
    SYSTEM_STATUS_CHECK
};

// Error structure
struct Error {
    std::string code;
    std::string message;
};

// Command message structure
struct Command {
    std::string protocolVersion;
    MessageKind kind;
    std::string commandId;
    CommandType type;
    int64_t timestampMs;
    std::map<std::string, std::string> payload;
};

// Response message structure
struct Response {
    std::string protocolVersion;
    MessageKind kind;
    std::string commandId;
    ResponseStatus status;
    int64_t timestampMs;
    std::map<std::string, std::string> result;
    std::shared_ptr<Error> error;
};

// Event message structure
struct Event {
    std::string protocolVersion;
    MessageKind kind;
    std::string eventId;
    EventType eventType;
    int64_t timestampMs;
    std::string deviceType;
    std::map<std::string, std::string> data;
};

// Helper functions for string conversion
inline std::string commandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::PAYMENT_START: return "payment_start";
        case CommandType::PAYMENT_CANCEL: return "payment_cancel";
        case CommandType::PAYMENT_TRANSACTION_CANCEL: return "payment_transaction_cancel";
        case CommandType::PAYMENT_STATUS: return "payment_status";
        case CommandType::PAYMENT_RESET: return "payment_reset";
        case CommandType::PAYMENT_DEVICE_CHECK: return "payment_device_check";
        case CommandType::PAYMENT_CARD_UID_READ: return "payment_card_uid_read";
        case CommandType::PAYMENT_LAST_APPROVAL: return "payment_last_approval";
        case CommandType::PAYMENT_IC_CARD_CHECK: return "payment_ic_card_check";
        case CommandType::PAYMENT_SCREEN_SOUND_SETTING: return "payment_screen_sound_setting";
        case CommandType::GET_DEVICE_LIST: return "get_device_list";
        case CommandType::GET_STATE_SNAPSHOT: return "get_state_snapshot";
        default: return "unknown";
    }
}

inline CommandType stringToCommandType(const std::string& str) {
    if (str == "payment_start") return CommandType::PAYMENT_START;
    if (str == "payment_cancel") return CommandType::PAYMENT_CANCEL;
    if (str == "payment_transaction_cancel") return CommandType::PAYMENT_TRANSACTION_CANCEL;
    if (str == "payment_status") return CommandType::PAYMENT_STATUS;
    if (str == "payment_reset") return CommandType::PAYMENT_RESET;
    if (str == "payment_device_check") return CommandType::PAYMENT_DEVICE_CHECK;
    if (str == "payment_card_uid_read") return CommandType::PAYMENT_CARD_UID_READ;
    if (str == "payment_last_approval") return CommandType::PAYMENT_LAST_APPROVAL;
    if (str == "payment_ic_card_check") return CommandType::PAYMENT_IC_CARD_CHECK;
    if (str == "payment_screen_sound_setting") return CommandType::PAYMENT_SCREEN_SOUND_SETTING;
    if (str == "get_device_list") return CommandType::GET_DEVICE_LIST;
    if (str == "get_state_snapshot") return CommandType::GET_STATE_SNAPSHOT;
    return CommandType::PAYMENT_START; // Default
}

inline std::string responseStatusToString(ResponseStatus status) {
    switch (status) {
        case ResponseStatus::OK: return "ok";
        case ResponseStatus::FAILED: return "failed";
        case ResponseStatus::REJECTED: return "rejected";
        default: return "unknown";
    }
}

inline ResponseStatus stringToResponseStatus(const std::string& str) {
    if (str == "ok") return ResponseStatus::OK;
    if (str == "failed") return ResponseStatus::FAILED;
    if (str == "rejected") return ResponseStatus::REJECTED;
    return ResponseStatus::FAILED; // Default
}

inline std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::PAYMENT_COMPLETE: return "payment_complete";
        case EventType::PAYMENT_FAILED: return "payment_failed";
        case EventType::PAYMENT_CANCELLED: return "payment_cancelled";
        case EventType::DEVICE_STATE_CHANGED: return "device_state_changed";
        case EventType::SYSTEM_STATUS_CHECK: return "system_status_check";
        default: return "unknown";
    }
}

inline EventType stringToEventType(const std::string& str) {
    if (str == "payment_complete") return EventType::PAYMENT_COMPLETE;
    if (str == "payment_failed") return EventType::PAYMENT_FAILED;
    if (str == "payment_cancelled") return EventType::PAYMENT_CANCELLED;
    if (str == "device_state_changed") return EventType::DEVICE_STATE_CHANGED;
    if (str == "system_status_check") return EventType::SYSTEM_STATUS_CHECK;
    return EventType::PAYMENT_COMPLETE; // Default
}

inline std::string messageKindToString(MessageKind kind) {
    switch (kind) {
        case MessageKind::COMMAND: return "command";
        case MessageKind::RESPONSE: return "response";
        case MessageKind::EVENT: return "event";
        default: return "unknown";
    }
}

inline MessageKind stringToMessageKind(const std::string& str) {
    if (str == "command") return MessageKind::COMMAND;
    if (str == "response") return MessageKind::RESPONSE;
    if (str == "event") return MessageKind::EVENT;
    return MessageKind::COMMAND; // Default
}

} // namespace ipc
