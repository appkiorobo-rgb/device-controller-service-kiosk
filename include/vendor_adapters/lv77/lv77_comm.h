// include/vendor_adapters/lv77/lv77_comm.h
#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#include "vendor_adapters/lv77/lv77_protocol.h"
#include "vendor_adapters/smartro/serial_port.h"
#include <string>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>

namespace lv77 {

// ICT-104U: 9600 8E1
constexpr uint32_t LV77_BAUD = 9600;
constexpr uint8_t  LV77_PARITY_EVEN = 2;  // EVENPARITY for Windows DCB

// Callback: bill in escrow (amount from bill type); return true to accept, false to reject
using EscrowCallback = std::function<bool(uint32_t amount)>;
// Callback: bill stacked (accepted)
using BillStackedCallback = std::function<void(uint32_t amount)>;
// Callback: status from poll
using StatusCallback = std::function<void(uint8_t statusCode)>;

class Lv77Comm {
public:
    explicit Lv77Comm(smartro::SerialPort& port);
    ~Lv77Comm();

    bool open(const std::string& portName);
    void close();
    bool isOpen() const { return port_.isOpen(); }

    // Sync after power-up: if device sent 0x80, send 0x02 and wait for 0x8F (within 2 sec)
    bool syncAfterPowerUp(uint32_t timeoutMs = 2000);

    // Enable/Disable
    bool enable();
    bool disable();

    // Single poll: send 0x0C, read one response byte
    bool poll(uint8_t& responseByte, uint32_t timeoutMs = 500);

    // Reset: send 0x30, then device sends 0x80, we send 0x02, device sends 0x8F
    bool reset(uint32_t timeoutMs = 3000);

    // Escrow: accept (0x10) or reject (0x0F) current bill
    bool acceptBill();
    bool rejectBill();

    // Start background poll loop (sends 0x0C every pollIntervalMs); processes escrow and status
    void startPollLoop(uint32_t pollIntervalMs = 500);
    void stopPollLoop();

    void setEscrowCallback(EscrowCallback cb) { escrowCallback_ = std::move(cb); }
    void setBillStackedCallback(BillStackedCallback cb) { billStackedCallback_ = std::move(cb); }
    void setStatusCallback(StatusCallback cb) { statusCallback_ = std::move(cb); }

    std::string getLastError() const { return lastError_; }

private:
    smartro::SerialPort& port_;
    std::string lastError_;
    mutable std::mutex mutex_;

    std::atomic<bool> pollLoopRunning_{false};
    std::thread pollLoopThread_;
    uint32_t pollIntervalMs_{500};

    EscrowCallback escrowCallback_;
    BillStackedCallback billStackedCallback_;
    StatusCallback statusCallback_;

    enum class EscrowState { Idle, WaitingBillType, WaitingAcceptReject };
    EscrowState escrowState_{EscrowState::Idle};
    uint32_t escrowAmount_{0};

    void pollLoopThread();
    bool readByte(uint8_t& byte, uint32_t timeoutMs);
    void setError(const std::string& msg);
};

} // namespace lv77
