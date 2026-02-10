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
    GET_STATE_SNAPSHOT,
    GET_CONFIG,
    SET_CONFIG,
    PRINTER_PRINT,
    CAMERA_CAPTURE,
    CAMERA_SET_SESSION,
    CAMERA_STATUS,
    CAMERA_START_PREVIEW,
    CAMERA_STOP_PREVIEW,
    CAMERA_SET_SETTINGS,
    CAMERA_RECONNECT,
    DETECT_HARDWARE,
    GET_AVAILABLE_PRINTERS,
    CASH_TEST_START,
    CASH_PAYMENT_START
};

// Event types
enum class EventType {
    PAYMENT_COMPLETE,
    PAYMENT_FAILED,
    PAYMENT_CANCELLED,
    DEVICE_STATE_CHANGED,
    SYSTEM_STATUS_CHECK,
    CAMERA_CAPTURE_COMPLETE,
    CAMERA_STATE_CHANGED,
    PRINTER_JOB_COMPLETE,
    CASH_TEST_AMOUNT,
    CASH_PAYMENT_TARGET_REACHED,
    CASH_BILL_STACKED
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

// Undef common Windows macros that can break member names (winres.h, winerror.h, etc.)
#ifdef result
#undef result
#endif
#ifdef response
#undef response
#endif
#ifdef Data
#undef Data
#endif
#ifdef ERROR
#undef ERROR
#endif

// Response message structure
struct Response {
    std::string protocolVersion;
    MessageKind kind;
    std::string commandId;
    ResponseStatus status;
    int64_t timestampMs;
    std::map<std::string, std::string> responseMap;
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
        case CommandType::GET_CONFIG: return "get_config";
        case CommandType::SET_CONFIG: return "set_config";
        case CommandType::PRINTER_PRINT: return "printer_print";
        case CommandType::CAMERA_CAPTURE: return "camera_capture";
        case CommandType::CAMERA_SET_SESSION: return "camera_set_session";
        case CommandType::CAMERA_STATUS: return "camera_status";
        case CommandType::CAMERA_START_PREVIEW: return "camera_start_preview";
        case CommandType::CAMERA_STOP_PREVIEW: return "camera_stop_preview";
        case CommandType::CAMERA_SET_SETTINGS: return "camera_set_settings";
        case CommandType::CAMERA_RECONNECT: return "camera_reconnect";
        case CommandType::DETECT_HARDWARE: return "detect_hardware";
        case CommandType::GET_AVAILABLE_PRINTERS: return "get_available_printers";
        case CommandType::CASH_TEST_START: return "cash_test_start";
        case CommandType::CASH_PAYMENT_START: return "cash_payment_start";
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
    if (str == "get_config") return CommandType::GET_CONFIG;
    if (str == "set_config") return CommandType::SET_CONFIG;
    if (str == "printer_print") return CommandType::PRINTER_PRINT;
    if (str == "camera_capture") return CommandType::CAMERA_CAPTURE;
    if (str == "camera_set_session") return CommandType::CAMERA_SET_SESSION;
    if (str == "camera_status") return CommandType::CAMERA_STATUS;
    if (str == "camera_start_preview") return CommandType::CAMERA_START_PREVIEW;
    if (str == "camera_stop_preview") return CommandType::CAMERA_STOP_PREVIEW;
    if (str == "camera_set_settings") return CommandType::CAMERA_SET_SETTINGS;
    if (str == "camera_reconnect") return CommandType::CAMERA_RECONNECT;
    if (str == "detect_hardware") return CommandType::DETECT_HARDWARE;
    if (str == "get_available_printers") return CommandType::GET_AVAILABLE_PRINTERS;
    if (str == "cash_test_start") return CommandType::CASH_TEST_START;
    if (str == "cash_payment_start") return CommandType::CASH_PAYMENT_START;
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
        case EventType::CAMERA_CAPTURE_COMPLETE: return "camera_capture_complete";
        case EventType::CAMERA_STATE_CHANGED: return "camera_state_changed";
        case EventType::PRINTER_JOB_COMPLETE: return "printer_job_complete";
        case EventType::CASH_TEST_AMOUNT: return "cash_test_amount";
        case EventType::CASH_PAYMENT_TARGET_REACHED: return "cash_payment_target_reached";
        case EventType::CASH_BILL_STACKED: return "cash_bill_stacked";
        default: return "unknown";
    }
}

inline EventType stringToEventType(const std::string& str) {
    if (str == "payment_complete") return EventType::PAYMENT_COMPLETE;
    if (str == "payment_failed") return EventType::PAYMENT_FAILED;
    if (str == "payment_cancelled") return EventType::PAYMENT_CANCELLED;
    if (str == "device_state_changed") return EventType::DEVICE_STATE_CHANGED;
    if (str == "system_status_check") return EventType::SYSTEM_STATUS_CHECK;
    if (str == "camera_capture_complete") return EventType::CAMERA_CAPTURE_COMPLETE;
    if (str == "camera_state_changed") return EventType::CAMERA_STATE_CHANGED;
    if (str == "printer_job_complete") return EventType::PRINTER_JOB_COMPLETE;
    if (str == "cash_test_amount") return EventType::CASH_TEST_AMOUNT;
    if (str == "cash_payment_target_reached") return EventType::CASH_PAYMENT_TARGET_REACHED;
    if (str == "cash_bill_stacked") return EventType::CASH_BILL_STACKED;
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
