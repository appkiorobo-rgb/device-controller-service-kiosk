// src/service_core/recovery_manager.cpp
#include "service_core/recovery_manager.h"
#include <algorithm>
#include <thread>
#include <chrono>

namespace device_controller {

RecoveryManager::RecoveryManager(const RecoveryConfig& config)
    : config_(config)
{
}

RecoveryManager::~RecoveryManager() {
}

void RecoveryManager::registerDevice(const std::string& deviceId, RecoveryAction recoveryAction) {
    std::lock_guard<std::mutex> lock(mutex_);
    DeviceRecoveryState state;
    state.recoveryAction = recoveryAction;
    state.currentBackoff = config_.initialBackoff;
    deviceStates_[deviceId] = state;
}

void RecoveryManager::reportFailure(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = deviceStates_.find(deviceId);
    if (it == deviceStates_.end()) {
        return;
    }

    auto& state = it->second;
    state.failureCount++;
    state.currentBackoff = calculateBackoff(state.failureCount);

    // Trigger recovery if max retries not exceeded
    if (state.failureCount <= config_.maxRetries) {
        // Schedule recovery with backoff
        std::thread([this, deviceId, backoff = state.currentBackoff]() {
            std::this_thread::sleep_for(backoff);
            triggerRecovery(deviceId);
        }).detach();
    }
}

void RecoveryManager::reportSuccess(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = deviceStates_.find(deviceId);
    if (it != deviceStates_.end()) {
        it->second.failureCount = 0;
        it->second.currentBackoff = config_.initialBackoff;
        it->second.lastActivityTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    }
}

bool RecoveryManager::checkHung(const std::string& deviceId, std::chrono::milliseconds lastActivityTime) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = deviceStates_.find(deviceId);
    if (it == deviceStates_.end()) {
        return false;
    }

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
    auto elapsed = now - lastActivityTime;
    
    return elapsed > config_.hungTimeout;
}

void RecoveryManager::triggerRecovery(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = deviceStates_.find(deviceId);
    if (it != deviceStates_.end() && it->second.recoveryAction) {
        it->second.recoveryAction();
    }
}

std::chrono::milliseconds RecoveryManager::calculateBackoff(int failureCount) const {
    auto backoff = config_.initialBackoff;
    for (int i = 1; i < failureCount; i++) {
        backoff = std::chrono::duration_cast<std::chrono::milliseconds>(
            backoff * config_.backoffMultiplier);
        if (backoff > config_.maxBackoff) {
            backoff = config_.maxBackoff;
            break;
        }
    }
    return backoff;
}

} // namespace device_controller
