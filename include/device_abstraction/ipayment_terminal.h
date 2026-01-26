// include/device_abstraction/ipayment_terminal.h
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <chrono>

namespace device_controller {

// Payment terminal state machine
enum class PaymentTerminalState {
    DISCONNECTED,
    CONNECTING,
    READY,
    PROCESSING,
    ERROR,
    HUNG
};

// Payment event types
enum class PaymentEventType {
    STATE_CHANGED,
    PAYMENT_COMPLETE,
    PAYMENT_FAILED,
    PAYMENT_CANCELLED,
    ERROR_OCCURRED
};

// Payment event data
struct PaymentEvent {
    PaymentEventType type;
    PaymentTerminalState state;
    std::string errorCode;
    std::string errorMessage;
    std::string transactionId;
    int64_t amount;  // Amount in smallest currency unit
    std::chrono::milliseconds timestamp;
};

// Event callback type
using PaymentEventCallback = std::function<void(const PaymentEvent&)>;

// IPaymentTerminal interface - stable abstraction for payment terminal devices
class IPaymentTerminal {
public:
    virtual ~IPaymentTerminal() = default;

    // Get current state
    virtual PaymentTerminalState getState() const = 0;

    // Initialize payment terminal connection
    // Returns true if initialization started, false if already initialized or error
    virtual bool initialize() = 0;

    // Shutdown payment terminal connection
    virtual void shutdown() = 0;

    // Start payment transaction
    // amount: amount in smallest currency unit
    // Does not return success/failure directly - result comes via event callback
    // Returns true if payment started, false if rejected
    virtual bool startPayment(int64_t amount) = 0;

    // Cancel ongoing payment transaction
    virtual void cancelPayment() = 0;

    // Reset payment terminal (vendor-specific implementation)
    virtual bool reset() = 0;

    // Register event callback
    virtual void setEventCallback(PaymentEventCallback callback) = 0;

    // Get device information
    virtual std::string getDeviceId() const = 0;
    virtual std::string getDeviceName() const = 0;
};

} // namespace device_controller
