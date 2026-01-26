// src/service_core/service_core.cpp
#include "service_core/service_core.h"
#include "service_core/device_orchestrator.h"
#include "service_core/recovery_manager.h"
#include "vendor_adapters/smartro/smartro_payment_terminal.h"
#include <memory>
#include <iostream>
#include <iostream>

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
    std::cout << "[SERVICE_CORE] Registering SMARTRO payment terminal..." << std::endl;
    // Register SMARTRO payment terminal
    auto paymentTerminal = std::make_shared<vendor::smartro::SMARTROPaymentTerminal>();
    orchestrator_->registerPaymentTerminal(paymentTerminal);
    std::cout << "[SERVICE_CORE] Payment terminal registered" << std::endl;
    
    // Initialize all devices
    std::cout << "[SERVICE_CORE] Initializing all devices..." << std::endl;
    orchestrator_->initializeAll();
    std::cout << "[SERVICE_CORE] Initialization complete" << std::endl;
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
