// include/service_core/recovery_manager.h
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace device_controller {

// RecoveryManager - handles reconnect, backoff, and hung detection
// Part of Service Core layer
class RecoveryManager {
public:
    using RecoveryAction = std::function<void()>;

    struct RecoveryConfig {
        std::chrono::milliseconds initialBackoff{1000};
        std::chrono::milliseconds maxBackoff{30000};
        double backoffMultiplier{2.0};
        int maxRetries{5};
        std::chrono::milliseconds hungTimeout{60000};  // 60 seconds default
    };

    RecoveryManager(const RecoveryConfig& config = RecoveryConfig());
    ~RecoveryManager();

    // Register a device for recovery management
    // deviceId: unique identifier for the device
    // recoveryAction: function to call when recovery is needed
    void registerDevice(const std::string& deviceId, RecoveryAction recoveryAction);

    // Report device failure
    void reportFailure(const std::string& deviceId);

    // Report device success (resets failure count)
    void reportSuccess(const std::string& deviceId);

    // Check if device is hung (no progress within timeout)
    // Returns true if device should be considered hung
    bool checkHung(const std::string& deviceId, std::chrono::milliseconds lastActivityTime);

    // Trigger recovery for a device
    void triggerRecovery(const std::string& deviceId);

private:
    struct DeviceRecoveryState {
        int failureCount{0};
        std::chrono::milliseconds currentBackoff{1000};
        std::chrono::milliseconds lastActivityTime{0};
        RecoveryAction recoveryAction;
    };

    std::mutex mutex_;
    RecoveryConfig config_;
    std::unordered_map<std::string, DeviceRecoveryState> deviceStates_;

    std::chrono::milliseconds calculateBackoff(int failureCount) const;
};

} // namespace device_controller
