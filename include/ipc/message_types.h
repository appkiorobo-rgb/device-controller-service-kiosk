// include/ipc/message_types.h
#pragma once

#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

namespace device_controller::ipc {

// Protocol version
constexpr const char* PROTOCOL_VERSION = "1.0";

// Message kinds
constexpr const char* MSG_KIND_COMMAND = "command";
constexpr const char* MSG_KIND_RESPONSE = "response";
constexpr const char* MSG_KIND_EVENT = "event";
constexpr const char* MSG_KIND_SNAPSHOT_REQUEST = "snapshot_request";
constexpr const char* MSG_KIND_SNAPSHOT_RESPONSE = "snapshot_response";

// Status values
constexpr const char* STATUS_OK = "OK";
constexpr const char* STATUS_REJECTED = "REJECTED";
constexpr const char* STATUS_FAILED = "FAILED";

// Device types
constexpr const char* DEVICE_TYPE_CAMERA = "camera";
constexpr const char* DEVICE_TYPE_PRINTER = "printer";
constexpr const char* DEVICE_TYPE_PAYMENT = "payment";

// Base message structure
struct Message {
    std::string protocolVersion;
    std::string kind;
    int64_t timestampMs;

    Message() : protocolVersion(PROTOCOL_VERSION), timestampMs(0) {}
    virtual ~Message() = default;
    virtual nlohmann::json toJson() const = 0;
    virtual bool fromJson(const nlohmann::json& json) = 0;
};

// Command message
struct Command : public Message {
    std::string commandId;
    std::string type;
    nlohmann::json payload;

    Command() {
        kind = MSG_KIND_COMMAND;
    }

    nlohmann::json toJson() const override {
        return {
            {"protocolVersion", protocolVersion},
            {"kind", kind},
            {"commandId", commandId},
            {"type", type},
            {"timestampMs", timestampMs},
            {"payload", payload}
        };
    }

    bool fromJson(const nlohmann::json& json) override {
        try {
            protocolVersion = json.value("protocolVersion", PROTOCOL_VERSION);
            kind = json.value("kind", MSG_KIND_COMMAND);
            commandId = json.value("commandId", "");
            type = json.value("type", "");
            timestampMs = json.value("timestampMs", 0LL);
            payload = json.value("payload", nlohmann::json::object());
            return true;
        } catch (...) {
            return false;
        }
    }
};

// Response message
struct Response : public Message {
    std::string commandId;
    std::string status;
    nlohmann::json error;  // null or error object
    nlohmann::json result;

    Response() {
        kind = MSG_KIND_RESPONSE;
        status = STATUS_OK;
    }

    nlohmann::json toJson() const override {
        auto json = nlohmann::json{
            {"protocolVersion", protocolVersion},
            {"kind", kind},
            {"commandId", commandId},
            {"status", status},
            {"timestampMs", timestampMs},
            {"result", result}
        };
        if (!error.is_null()) {
            json["error"] = error;
        } else {
            json["error"] = nullptr;
        }
        return json;
    }

    bool fromJson(const nlohmann::json& json) override {
        try {
            protocolVersion = json.value("protocolVersion", PROTOCOL_VERSION);
            kind = json.value("kind", MSG_KIND_RESPONSE);
            commandId = json.value("commandId", "");
            status = json.value("status", STATUS_OK);
            timestampMs = json.value("timestampMs", 0LL);
            error = json.value("error", nlohmann::json());
            result = json.value("result", nlohmann::json::object());
            return true;
        } catch (...) {
            return false;
        }
    }
};

// Event message
struct Event : public Message {
    std::string eventId;
    std::string eventType;
    std::string deviceType;
    nlohmann::json data;

    Event() {
        kind = MSG_KIND_EVENT;
    }

    nlohmann::json toJson() const override {
        return {
            {"protocolVersion", protocolVersion},
            {"kind", kind},
            {"eventId", eventId},
            {"eventType", eventType},
            {"timestampMs", timestampMs},
            {"deviceType", deviceType},
            {"data", data}
        };
    }

    bool fromJson(const nlohmann::json& json) override {
        try {
            protocolVersion = json.value("protocolVersion", PROTOCOL_VERSION);
            kind = json.value("kind", MSG_KIND_EVENT);
            eventId = json.value("eventId", "");
            eventType = json.value("eventType", "");
            timestampMs = json.value("timestampMs", 0LL);
            deviceType = json.value("deviceType", "");
            data = json.value("data", nlohmann::json::object());
            return true;
        } catch (...) {
            return false;
        }
    }
};

// Snapshot request
struct SnapshotRequest : public Message {
    std::string requestId;
    std::vector<std::string> deviceTypes;

    SnapshotRequest() {
        kind = MSG_KIND_SNAPSHOT_REQUEST;
    }

    nlohmann::json toJson() const override {
        return {
            {"protocolVersion", protocolVersion},
            {"kind", kind},
            {"requestId", requestId},
            {"timestampMs", timestampMs},
            {"deviceTypes", deviceTypes}
        };
    }

    bool fromJson(const nlohmann::json& json) override {
        try {
            protocolVersion = json.value("protocolVersion", PROTOCOL_VERSION);
            kind = json.value("kind", MSG_KIND_SNAPSHOT_REQUEST);
            requestId = json.value("requestId", "");
            timestampMs = json.value("timestampMs", 0LL);
            deviceTypes = json.value("deviceTypes", std::vector<std::string>());
            return true;
        } catch (...) {
            return false;
        }
    }
};

// Snapshot response
struct SnapshotResponse : public Message {
    std::string requestId;
    nlohmann::json snapshot;

    SnapshotResponse() {
        kind = MSG_KIND_SNAPSHOT_RESPONSE;
    }

    nlohmann::json toJson() const override {
        return {
            {"protocolVersion", protocolVersion},
            {"kind", kind},
            {"requestId", requestId},
            {"timestampMs", timestampMs},
            {"snapshot", snapshot}
        };
    }

    bool fromJson(const nlohmann::json& json) override {
        try {
            protocolVersion = json.value("protocolVersion", PROTOCOL_VERSION);
            kind = json.value("kind", MSG_KIND_SNAPSHOT_RESPONSE);
            requestId = json.value("requestId", "");
            timestampMs = json.value("timestampMs", 0LL);
            snapshot = json.value("snapshot", nlohmann::json::object());
            return true;
        } catch (...) {
            return false;
        }
    }
};

} // namespace device_controller::ipc
