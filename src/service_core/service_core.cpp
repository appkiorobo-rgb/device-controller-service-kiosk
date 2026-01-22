// src/service_core/service_core.cpp
#include "service_core/service_core.h"
#include "service_core/device_orchestrator.h"
#include "service_core/recovery_manager.h"
#include <memory>

namespace device_controller {

ServiceCore::ServiceCore()
    : orchestrator_(std::make_shared<DeviceOrchestrator>())
    , recoveryManager_(std::make_unique<RecoveryManager>())
{
}

ServiceCore::~ServiceCore() {
    shutdown();
}

bool ServiceCore::initialize() {
    // TODO: Register devices from vendor adapters
    // For now, this is a placeholder
    
    orchestrator_->initializeAll();
    return true;
}

void ServiceCore::shutdown() {
    if (orchestrator_) {
        orchestrator_->shutdownAll();
    }
}

std::shared_ptr<DeviceOrchestrator> ServiceCore::getOrchestrator() {
    return orchestrator_;
}

} // namespace device_controller
