// src/main/service_main.cpp
#include "service_core/service_core.h"
#include "ipc/named_pipe_server.h"
#include "ipc/command_processor.h"
#include "ipc/message_types.h"
#include "logging/logger.h"
#include "common/uuid_generator.h"
#include <windows.h>
#include <iostream>
#include <memory>
#include <string>

using namespace device_controller;
using namespace device_controller::ipc;
using namespace device_controller::logging;

// Service name
constexpr const char* SERVICE_NAME = "DeviceControllerService";
constexpr const char* PIPE_NAME = "\\\\.\\pipe\\DeviceControllerService";

// Global service state
SERVICE_STATUS g_serviceStatus;
SERVICE_STATUS_HANDLE g_serviceStatusHandle;
std::unique_ptr<ServiceCore> g_serviceCore;
std::unique_ptr<NamedPipeServer> g_pipeServer;
std::unique_ptr<Logger> g_logger;

// Windows Service control handler
void WINAPI ServiceCtrlHandler(DWORD dwCtrl) {
    switch (dwCtrl) {
        case SERVICE_CONTROL_STOP:
            g_serviceStatus.dwWin32ExitCode = 0;
            g_serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
            
            if (g_pipeServer) {
                g_pipeServer->stop();
            }
            if (g_serviceCore) {
                g_serviceCore->shutdown();
            }
            break;
        default:
            break;
    }
}

// Windows Service main function
void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_serviceStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_serviceStatusHandle) {
        return;
    }

    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_serviceStatus.dwWin32ExitCode = 0;
    g_serviceStatus.dwServiceSpecificExitCode = 0;
    g_serviceStatus.dwCheckPoint = 0;
    g_serviceStatus.dwWaitHint = 0;
    SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);

    try {
        // Initialize logging
        g_logger = std::make_unique<Logger>("C:\\ProgramData\\DeviceControllerService\\logs", "service.log");
        g_logger->info("Device Controller Service starting...");

        // Initialize service core
        g_serviceCore = std::make_unique<ServiceCore>();
        if (!g_serviceCore->initialize()) {
            g_logger->error("Failed to initialize service core");
            g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
            return;
        }

        // Setup IPC message handler
        auto orchestrator = g_serviceCore->getOrchestrator();
        auto commandProcessor = std::make_shared<CommandProcessor>(orchestrator);
        
        NamedPipeServer::MessageHandler handler = [commandProcessor](const std::string& message, std::string& response) {
            try {
                auto json = nlohmann::json::parse(message);
                Command cmd;
                if (cmd.fromJson(json)) {
                    auto resp = commandProcessor->processCommand(cmd);
                    response = resp.toJson().dump();
                } else {
                    // Invalid command
                    Response errorResp;
                    errorResp.commandId = json.value("commandId", "");
                    errorResp.status = STATUS_REJECTED;
                    errorResp.error = {
                        {"code", "INVALID_MESSAGE"},
                        {"message", "Failed to parse command"}
                    };
                    response = errorResp.toJson().dump();
                }
            } catch (const std::exception& e) {
                Response errorResp;
                errorResp.status = STATUS_FAILED;
                errorResp.error = {
                    {"code", "PROCESSING_ERROR"},
                    {"message", e.what()}
                };
                response = errorResp.toJson().dump();
            }
        };

        // Start Named Pipe server
        g_pipeServer = std::make_unique<NamedPipeServer>(PIPE_NAME, handler);
        if (!g_pipeServer->start()) {
            g_logger->error("Failed to start Named Pipe server");
            g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
            return;
        }

        g_logger->info("Device Controller Service started successfully");
        g_serviceStatus.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);

        // Main service loop
        while (g_serviceStatus.dwCurrentState == SERVICE_RUNNING) {
            Sleep(1000);  // Check every second
        }

    } catch (const std::exception& e) {
        if (g_logger) {
            g_logger->error(std::string("Service error: ") + e.what());
        }
        g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
    }
}

// Install service
bool InstallService() {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scManager) {
        std::cerr << "Failed to open service control manager" << std::endl;
        return false;
    }

    char servicePath[MAX_PATH];
    GetModuleFileName(nullptr, servicePath, MAX_PATH);

    SC_HANDLE service = CreateService(
        scManager,
        SERVICE_NAME,
        "Device Controller Service",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        servicePath,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (!service) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            std::cerr << "Service already exists" << std::endl;
        } else {
            std::cerr << "Failed to create service. Error: " << error << std::endl;
        }
        CloseServiceHandle(scManager);
        return false;
    }

    // Set service description
    SERVICE_DESCRIPTION desc;
    desc.lpDescription = const_cast<LPSTR>("Hardware control platform for kiosk systems");
    ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &desc);

    std::cout << "Service installed successfully" << std::endl;
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}

// Uninstall service
bool UninstallService() {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        std::cerr << "Failed to open service control manager" << std::endl;
        return false;
    }

    SC_HANDLE service = OpenService(scManager, SERVICE_NAME, DELETE);
    if (!service) {
        std::cerr << "Failed to open service" << std::endl;
        CloseServiceHandle(scManager);
        return false;
    }

    if (!DeleteService(service)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to delete service. Error: " << error << std::endl;
        CloseServiceHandle(service);
        CloseServiceHandle(scManager);
        return false;
    }

    std::cout << "Service uninstalled successfully" << std::endl;
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}

// Entry point
int main(int argc, char* argv[]) {
    // Check for install/uninstall commands
    if (argc > 1) {
        std::string command = argv[1];
        if (command == "install") {
            return InstallService() ? 0 : 1;
        } else if (command == "uninstall") {
            return UninstallService() ? 0 : 1;
        } else {
            std::cerr << "Usage: " << argv[0] << " [install|uninstall]" << std::endl;
            return 1;
        }
    }

    // Normal service execution
    SERVICE_TABLE_ENTRY serviceTable[] = {
        { const_cast<LPSTR>(SERVICE_NAME), ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcher(serviceTable)) {
        std::cerr << "Failed to start service control dispatcher" << std::endl;
        return 1;
    }

    return 0;
}
