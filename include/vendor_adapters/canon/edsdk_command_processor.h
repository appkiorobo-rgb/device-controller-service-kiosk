// include/vendor_adapters/canon/edsdk_command_processor.h
#pragma once

// Include logger first to prevent Windows SDK conflicts
#include "logging/logger.h"

#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

// Forward declarations
class EdsdkCommand;

namespace canon {

// Command processor for async EDSDK operations
// Based on Canon sample code Processor pattern
class EdsdkCommandProcessor {
public:
    EdsdkCommandProcessor();
    ~EdsdkCommandProcessor();
    
    // Start processor thread
    bool start();
    
    // Stop processor thread
    void stop();
    
    // Wait for thread to finish
    void join();
    
    // Enqueue command for execution
    void enqueue(std::shared_ptr<EdsdkCommand> command);
    
    // Set close command (executed before stopping)
    void setCloseCommand(std::shared_ptr<EdsdkCommand> command);
    
    // Clear all pending commands
    void clear();
    
    // Check if running
    bool isRunning() const { return running_; }
    
private:
    void run();
    std::shared_ptr<EdsdkCommand> take();
    
    std::atomic<bool> running_;
    std::deque<std::shared_ptr<EdsdkCommand>> queue_;
    std::shared_ptr<EdsdkCommand> closeCommand_;
    
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread thread_;
};

} // namespace canon
