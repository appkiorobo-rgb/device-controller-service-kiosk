// include/service_core/service_core.h
#pragma once

#include "service_core/device_orchestrator.h"
#include "service_core/recovery_manager.h"
#include <memory>

namespace device_controller {

// ServiceCore - main orchestrator for the service
// Part of Service Core layer
class ServiceCore {
public:
    ServiceCore();
    ~ServiceCore();

    // Initialize service (register devices, start IPC, etc.)
    bool initialize();

    // Shutdown service
    void shutdown();

    // Get device orchestrator
    std::shared_ptr<DeviceOrchestrator> getOrchestrator();

private:
    std::shared_ptr<DeviceOrchestrator> orchestrator_;
    std::unique_ptr<RecoveryManager> recoveryManager_;
};

} // namespace device_controller
