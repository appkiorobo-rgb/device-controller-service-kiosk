// include/devices/ipayment_terminal.h
#pragma once

#include "devices/device_types.h"
#include <string>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace devices {

// Payment complete event data (full approval detail for server/store)
struct PaymentCompleteEvent {
    std::string transactionId;
    uint32_t amount = 0;
    std::string cardNumber;       // Masked card number
    std::string approvalNumber;
    std::string salesDate;        // YYYYMMDD
    std::string salesTime;        // hhmmss
    std::string transactionMedium; // IC/MS/RF/QR
    DeviceState state = DeviceState::DISCONNECTED;
    // Extended approval detail
    std::string status;           // e.g. "SUCCESS"
    std::string transactionType; // e.g. "Credit Approval"
    std::string approvalAmount;  // 10 bytes string from terminal
    std::string tax;
    std::string serviceCharge;
    std::string installments;
    std::string merchantNumber;
    std::string terminalNumber;
    std::string issuer;
    std::string acquirer;
};

// Payment failed event data
struct PaymentFailedEvent {
    std::string errorCode;
    std::string errorMessage;
    uint32_t amount = 0;
    DeviceState state = DeviceState::DISCONNECTED;
};

// Payment cancelled event data
struct PaymentCancelledEvent {
    DeviceState state = DeviceState::DISCONNECTED;
};

// --- Vendor-agnostic result/request types for extended operations ---

/// Result of reading a card UID (NFC/RF)
struct CardUidResult {
    bool success = false;
    std::vector<uint8_t> uid;
    std::string error;
};

/// Result of checking IC card insertion status
struct IcCardCheckResult {
    bool success = false;
    bool cardInserted = false;
    char cardStatus = 0; // 'O' = inserted, 'X' = not inserted (vendor may differ)
    std::string error;
};

/// Screen brightness and sound volume settings
struct ScreenSoundSettings {
    uint8_t screenBrightness = 0;
    uint8_t soundVolume = 0;
    uint8_t touchSoundVolume = 0;
};

/// Request to cancel a previous transaction
struct TransactionCancelRequest {
    std::string cancelType;       // e.g. "1" = request cancel, "2" = last transaction cancel
    std::string transactionType;  // e.g. "01" credit approval
    uint32_t amount = 0;
    std::string approvalNumber;
    std::string originalDate;     // YYYYMMDD
    std::string originalTime;     // hhmmss
    std::string tax;
    std::string service;
    std::string installments;
    std::string additionalInfo;
};

/// Result of a transaction cancellation
struct TransactionCancelResult {
    bool success = false;
    std::string transactionType;
    std::string transactionMedium;
    std::string cardNumber;
    std::string approvalAmount;
    std::string tax;
    std::string serviceCharge;
    std::string installments;
    std::string approvalNumber;
    std::string salesDate;
    std::string salesTime;
    std::string error;
};

// Payment terminal interface
class IPaymentTerminal {
public:
    virtual ~IPaymentTerminal() = default;

    // --- Core (pure virtual, all vendors must implement) ---

    virtual DeviceInfo getDeviceInfo() const = 0;
    virtual bool startPayment(uint32_t amount) = 0;
    virtual bool cancelPayment() = 0;
    virtual DeviceState getState() const = 0;
    virtual bool reset() = 0;
    virtual bool checkDevice() = 0;

    /// Vendor identifier (e.g. "smartro", "lv77"). Used for logging and auto-detect.
    virtual std::string getVendorName() const = 0;

    /// Current COM port this adapter is connected to.
    virtual std::string getComPort() const = 0;

    /// Close current port and reconnect on a different COM port.
    virtual bool reconnect(const std::string& newPort) = 0;

    // --- Event callbacks (pure virtual) ---

    virtual void setPaymentCompleteCallback(std::function<void(const PaymentCompleteEvent&)> callback) = 0;
    virtual void setPaymentFailedCallback(std::function<void(const PaymentFailedEvent&)> callback) = 0;
    virtual void setPaymentCancelledCallback(std::function<void(const PaymentCancelledEvent&)> callback) = 0;
    virtual void setStateChangedCallback(std::function<void(DeviceState)> callback) = 0;

    // --- Extended operations (virtual with default "not supported") ---
    // Vendors override only the methods they support.

    /// Read NFC/RF card UID.
    virtual CardUidResult readCardUid() {
        return {false, {}, "Not supported by this terminal"};
    }

    /// Check whether an IC card is inserted.
    virtual IcCardCheckResult checkIcCard() {
        return {false, false, 0, "Not supported by this terminal"};
    }

    /// Set screen brightness and sound volume.
    virtual bool setScreenSound(const ScreenSoundSettings& request, ScreenSoundSettings& response) {
        (void)request; (void)response;
        return false; // not supported
    }

    /// Cancel a previous transaction (refund).
    virtual TransactionCancelResult cancelTransaction(const TransactionCancelRequest& request) {
        (void)request;
        return {false, "", "", "", "", "", "", "", "", "", "", "Not supported by this terminal"};
    }

    /// Retrieve last approval details.
    virtual PaymentCompleteEvent getLastApproval(const std::string& transactionType) {
        (void)transactionType;
        return {}; // not supported
    }
};

} // namespace devices
