// include/core/service_core.h
#pragma once

#include "core/device_manager.h"
#include "ipc/ipc_server.h"
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

namespace core {

// Async task for device operations
struct DeviceTask {
    enum class Type {
        PAYMENT_START,
        PAYMENT_CANCEL,
        PAYMENT_RESET,
        PAYMENT_DEVICE_CHECK,
        // Add more task types as needed
    };
    
    Type type;
    std::string commandId;
    std::map<std::string, std::string> params;
    std::function<void()> execute;
};

// Service Core (device management + IPC)
class ServiceCore {
public:
    ServiceCore();
    ~ServiceCore();
    
    // Start service
    bool start();
    
    // Stop service
    void stop();
    
    // Access Device Manager
    DeviceManager& getDeviceManager() { return deviceManager_; }
    
    // Access IPC Server
    ipc::IpcServer& getIpcServer() { return ipcServer_; }
    
    bool isRunning() const { return running_; }
    
private:
    DeviceManager deviceManager_;
    ipc::IpcServer ipcServer_;
    bool running_;
    
    // Async task queue
    std::queue<DeviceTask> taskQueue_;
    std::mutex taskQueueMutex_;
    std::condition_variable taskQueueCondition_;
    std::atomic<bool> taskQueueRunning_;
    std::thread taskWorkerThread_;
    
    // Register IPC command handlers
    void registerCommandHandlers();
    
    // Setup event callbacks (for IPC event broadcasting)
    void setupEventCallbacks();
    
    // Task queue management
    void startTaskWorker();
    void stopTaskWorker();
    void taskWorkerThread();
    void enqueueTask(const DeviceTask& task);
    
    // Command handler implementations (synchronous - immediate response)
    ipc::Response handleGetStateSnapshot(const ipc::Command& cmd);
    ipc::Response handleGetDeviceList(const ipc::Command& cmd);
    ipc::Response handlePaymentStart(const ipc::Command& cmd);
    ipc::Response handlePaymentCancel(const ipc::Command& cmd);
    ipc::Response handlePaymentTransactionCancel(const ipc::Command& cmd);
    ipc::Response handlePaymentStatusCheck(const ipc::Command& cmd);
    ipc::Response handlePaymentReset(const ipc::Command& cmd);
    ipc::Response handlePaymentDeviceCheck(const ipc::Command& cmd);
    ipc::Response handlePaymentCardUidRead(const ipc::Command& cmd);
    ipc::Response handlePaymentLastApproval(const ipc::Command& cmd);
    ipc::Response handlePaymentIcCardCheck(const ipc::Command& cmd);
    ipc::Response handlePaymentScreenSoundSetting(const ipc::Command& cmd);
    
    // Camera command handlers
    ipc::Response handleCameraCapture(const ipc::Command& cmd);
    ipc::Response handleCameraSetSession(const ipc::Command& cmd);
    ipc::Response handleCameraStatus(const ipc::Command& cmd);
    ipc::Response handleCameraStartPreview(const ipc::Command& cmd);
    ipc::Response handleCameraStopPreview(const ipc::Command& cmd);
    ipc::Response handleCameraSetSettings(const ipc::Command& cmd);
    
    // Async task implementations (executed in worker thread)
    void executePaymentStart(const DeviceTask& task);
    void executePaymentCancel(const DeviceTask& task);
    void executePaymentReset(const DeviceTask& task);
    void executePaymentDeviceCheck(const DeviceTask& task);
    
    // Event publishing helpers
    void publishPaymentCompleteEvent(const devices::PaymentCompleteEvent& event);
    void publishPaymentFailedEvent(const devices::PaymentFailedEvent& event);
    void publishPaymentCancelledEvent(const devices::PaymentCancelledEvent& event);
    void publishDeviceStateChangedEvent(const std::string& deviceType, devices::DeviceState state);
    void publishSystemStatusCheckEvent(const std::map<std::string, devices::DeviceInfo>& deviceStatuses, bool allHealthy);
    void publishCameraCaptureCompleteEvent(const devices::CaptureCompleteEvent& event);
    
    // Status check on client connection
    void performSystemStatusCheck();
    
    // UUID generation
    std::string generateUUID();
};

} // namespace core
