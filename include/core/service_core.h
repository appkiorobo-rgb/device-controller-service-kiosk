// include/core/service_core.h
#pragma once

#include "core/device_manager.h"
#include "core/device_constants.h"
#include "devices/iprinter.h"
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

    /// Call after registering devices and before start(). Registers capture_complete etc. so events are sent.
    void prepareEventCallbacks();

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

    // Cash test mode (debug): accept bills and report total via CASH_TEST_AMOUNT event
    bool cashTestMode_;
    uint32_t cashTestTotal_;
    
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
    ipc::Response handleGetConfig(const ipc::Command& cmd);
    ipc::Response handleSetConfig(const ipc::Command& cmd);
    ipc::Response handlePrinterPrint(const ipc::Command& cmd);
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
    ipc::Response handleCameraReconnect(const ipc::Command& cmd);
    ipc::Response handleDetectHardware(const ipc::Command& cmd);
    ipc::Response handleGetAvailablePrinters(const ipc::Command& cmd);
    ipc::Response handleCashTestStart(const ipc::Command& cmd);
    ipc::Response handleCashPaymentStart(const ipc::Command& cmd);

    /// 자동감지(detect_hardware) 전에 READY가 아닌 장치에 대해 재연결 시도. 호출 후 handleDetectHardware로 상태 수집.
    /// payloadOverrides: command payload로 enable 플래그 오버라이드 가능 (비어있으면 config에서 읽음).
    void tryReconnectDevicesBeforeDetect(
        const std::map<std::string, std::string>& payloadOverrides = {});

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
    void publishPrinterJobCompleteEvent(const devices::PrintJobCompleteEvent& event);
    void publishCashTestAmountEvent(uint32_t totalAmount);
    void publishCashPaymentTargetReachedEvent(uint32_t totalAmount);
    void publishCashBillStackedEvent(uint32_t amount, uint32_t currentTotal);

    // Status check on client connection
    void performSystemStatusCheck();
    
    /// Called when pipe client disconnects: cancel payment, stop liveview, etc.
    void resetOnClientDisconnect();
    
    // UUID generation
    std::string generateUUID();
};

} // namespace core
