// src/vendor_adapters/canon/edsdk_command_processor.cpp
// Include logger first to prevent Windows SDK conflicts
#include "logging/logger.h"
#include "devices/device_types.h"
#include "vendor_adapters/canon/edsdk_wrapper.h"

#include "vendor_adapters/canon/edsdk_command_processor.h"
#include "vendor_adapters/canon/edsdk_commands.h"
#include <windows.h>
#include <comdef.h>
#include <thread>
#include <chrono>

namespace canon {

EdsdkCommandProcessor::EdsdkCommandProcessor()
    : running_(false)
    , closeCommand_(nullptr) {
}

EdsdkCommandProcessor::~EdsdkCommandProcessor() {
    stop();
    clear();
}

bool EdsdkCommandProcessor::start() {
    if (running_) {
        return true;
    }
    
    running_ = true;
    thread_ = std::thread(&EdsdkCommandProcessor::run, this);
    
    logging::Logger::getInstance().info("EDSDK Command Processor started");
    return true;
}

void EdsdkCommandProcessor::stop() {
    if (!running_) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    condition_.notify_all();
}

void EdsdkCommandProcessor::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void EdsdkCommandProcessor::enqueue(std::shared_ptr<EdsdkCommand> command) {
    if (!command) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(command);
    }
    condition_.notify_one();
}

void EdsdkCommandProcessor::setCloseCommand(std::shared_ptr<EdsdkCommand> command) {
    closeCommand_ = command;
}

void EdsdkCommandProcessor::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
}

void EdsdkCommandProcessor::run() {
    // Initialize COM for this thread (required for EDSDK on Windows)
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        logging::Logger::getInstance().error("Failed to initialize COM in command processor thread");
        running_ = false;
        return;
    }
    
    // (2) EDSDK: All EDSDK calls on this single thread (handler registration, EdsGetEvent pump, EdsSendCommand, EdsDownload).
    logging::Logger::getInstance().info("EDSDK Command Processor thread running (single thread for EDSDK)");
    
    while (running_) {
        std::shared_ptr<EdsdkCommand> command = take();
        
        if (command) {
            bool complete = command->execute();
            
            if (!complete) {
                // Command failed but should retry (e.g., DeviceBusy)
                // Wait before retrying to avoid camera instability
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                enqueue(command);
            }
        } else {
            // EdsGetEvent() pump - run regularly or callbacks never fire (no per-tick logging)
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            for (int i = 0; i < 10; i++) {
                if (EdsGetEvent() != EDS_ERR_OK) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // Clear remaining commands
    clear();
    
    // Execute close command if set
    if (closeCommand_) {
        logging::Logger::getInstance().info("Executing close command");
        closeCommand_->execute();
        closeCommand_.reset();
    }
    
    CoUninitialize();
    logging::Logger::getInstance().info("EDSDK Command Processor thread exiting");
}

std::shared_ptr<EdsdkCommand> EdsdkCommandProcessor::take() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait for command or stop signal (10ms timeout)
    bool hasCommand = condition_.wait_for(lock, std::chrono::milliseconds(10), [this] {
        return !queue_.empty() || !running_;
    });
    
    if (!running_ && queue_.empty()) {
        return nullptr;
    }
    
    if (!queue_.empty()) {
        std::shared_ptr<EdsdkCommand> command = queue_.front();
        queue_.pop_front();
        return command;
    }
    
    // Queue is empty and running_ is true - return nullptr so EdsGetEvent() pump runs
    return nullptr;
}

} // namespace canon
