// include/devices/ipayment_terminal.h
#pragma once

#include "devices/device_types.h"
#include <string>
#include <cstdint>
#include <functional>
#include <memory>

namespace devices {

// Payment complete event data
struct PaymentCompleteEvent {
    std::string transactionId;
    uint32_t amount;
    std::string cardNumber;      // Masked card number
    std::string approvalNumber;
    std::string salesDate;        // YYYYMMDD
    std::string salesTime;        // hhmmss
    std::string transactionMedium; // IC/MS/RF/QR
    DeviceState state;            // Usually returns to READY
};

// Payment failed event data
struct PaymentFailedEvent {
    std::string errorCode;
    std::string errorMessage;
    uint32_t amount;
    DeviceState state;
};

// Payment cancelled event data
struct PaymentCancelledEvent {
    DeviceState state;  // Usually returns to READY
};

// Payment terminal interface
class IPaymentTerminal {
public:
    virtual ~IPaymentTerminal() = default;

    // Get device information
    virtual DeviceInfo getDeviceInfo() const = 0;

    // Start payment (async)
    // Result is received via event (payment_complete or payment_failed)
    virtual bool startPayment(uint32_t amount) = 0;

    // Cancel payment
    virtual bool cancelPayment() = 0;

    // Check state
    virtual DeviceState getState() const = 0;

    // Reset terminal
    virtual bool reset() = 0;

    // Hardware check
    virtual bool checkDevice() = 0;

    // Set event callbacks
    virtual void setPaymentCompleteCallback(std::function<void(const PaymentCompleteEvent&)> callback) = 0;
    virtual void setPaymentFailedCallback(std::function<void(const PaymentFailedEvent&)> callback) = 0;
    virtual void setPaymentCancelledCallback(std::function<void(const PaymentCancelledEvent&)> callback) = 0;
    virtual void setStateChangedCallback(std::function<void(DeviceState)> callback) = 0;
};

} // namespace devices
