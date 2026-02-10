// src/vendor_adapters/lv77/lv77_comm.cpp
#include "logging/logger.h"
#include "vendor_adapters/lv77/lv77_comm.h"
#include <chrono>
#include <thread>

namespace lv77 {

Lv77Comm::Lv77Comm(smartro::SerialPort& port) : port_(port) {}

Lv77Comm::~Lv77Comm() {
    stopPollLoop();
    close();
}

void Lv77Comm::setError(const std::string& msg) {
    lastError_ = msg;
    logging::Logger::getInstance().warn("[LV77] " + msg);
}

bool Lv77Comm::open(const std::string& portName) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();
    if (port_.isOpen()) port_.close();
    if (!port_.open(portName, LV77_BAUD)) {
        setError("Failed to open port: " + portName);
        return false;
    }
    if (!port_.setParity(LV77_PARITY_EVEN)) {
        setError("Failed to set 8E1 parity");
        port_.close();
        return false;
    }
    logging::Logger::getInstance().info("[LV77] Opened " + portName + " at " + std::to_string(LV77_BAUD) + " 8E1");
    return true;
}

void Lv77Comm::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (port_.isOpen()) port_.close();
}

bool Lv77Comm::readByte(uint8_t& byte, uint32_t timeoutMs) {
    size_t n = 0;
    if (!port_.read(&byte, 1, n, timeoutMs) || n == 0) return false;
    return true;
}

bool Lv77Comm::syncAfterPowerUp(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();

    // Protocol: device sends 0x80 on power-up; we send 0x02 within 2 sec; device replies 0x8F.
    // If device was already on, we may have missed 0x80. Drain any pending byte first.
    uint8_t rsp = 0;
    if (readByte(rsp, 300)) {
        if (rsp == RSP_POWER_UP) {
            logging::Logger::getInstance().info("[LV77] Received 0x80 (power-up), sending 0x02");
        }
        // else: discard unexpected byte and continue
    }

    uint8_t cmd = CMD_SYNC_ACK;
    if (!port_.write(&cmd, 1)) {
        setError("Failed to send sync 0x02");
        return false;
    }
    if (!readByte(rsp, timeoutMs)) {
        // No 0x8F - device may already be on and not in sync state. Continue anyway.
        logging::Logger::getInstance().warn("[LV77] Sync: no 0x8F (device may already be on). Proceeding.");
        lastError_.clear();
        return true;
    }
    if (rsp != RSP_SYNC_OK) {
        logging::Logger::getInstance().warn("[LV77] Sync: unexpected 0x" + std::to_string(static_cast<int>(rsp)) + ", proceeding.");
        lastError_.clear();
        return true;
    }
    // Protocol: 0x8F followed by Country Code1 (ASCII), Country Code2 (ASCII). Read and discard so buffer is clean.
    uint8_t cc1 = 0, cc2 = 0;
    if (readByte(cc1, 200)) readByte(cc2, 200);
    logging::Logger::getInstance().info("[LV77] Sync OK (0x8F)" +
        (cc1 || cc2 ? std::string(" Country: ") + static_cast<char>(cc1 ? cc1 : '?') + static_cast<char>(cc2 ? cc2 : '?') : ""));
    return true;
}

bool Lv77Comm::enable() {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();
    uint8_t cmd = CMD_ENABLE;
    if (!port_.write(&cmd, 1)) {
        setError("Failed to send enable 0x3E");
        return false;
    }
    logging::Logger::getInstance().info("[LV77] Enable sent");
    return true;
}

bool Lv77Comm::disable() {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();
    uint8_t cmd = CMD_DISABLE;  // 0x5E — 현금결제기 DISABLE (다음 사용 시 enable 0x3E 전송)
    if (!port_.write(&cmd, 1)) {
        setError("Failed to send disable 0x5E");
        return false;
    }
    logging::Logger::getInstance().info("[LV77] Disable (0x5E) sent");
    return true;
}

bool Lv77Comm::poll(uint8_t& responseByte, uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();
    uint8_t cmd = CMD_POLL_STATUS;
    if (!port_.write(&cmd, 1)) {
        setError("Failed to send poll");
        return false;
    }
    if (!readByte(responseByte, timeoutMs)) {
        setError("Poll: no response");
        return false;
    }
    return true;
}

bool Lv77Comm::reset(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();
    uint8_t cmd = CMD_RESET;
    if (!port_.write(&cmd, 1)) {
        setError("Failed to send reset");
        return false;
    }
    uint8_t rsp = 0;
    if (!readByte(rsp, timeoutMs)) {
        setError("Reset: no response (expected 0x80)");
        return false;
    }
    if (rsp != RSP_POWER_UP) {
        setError("Reset: expected 0x80, got 0x" + std::to_string(rsp));
        return false;
    }
    uint8_t sync = CMD_SYNC_ACK;
    if (!port_.write(&sync, 1)) {
        setError("Reset: failed to send 0x02");
        return false;
    }
    if (!readByte(rsp, timeoutMs) || rsp != RSP_SYNC_OK) {
        setError("Reset: expected 0x8F after sync");
        return false;
    }
    logging::Logger::getInstance().info("[LV77] Reset OK");
    return true;
}

bool Lv77Comm::acceptBill() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint8_t cmd = CMD_ACCEPT_STACK;
    if (!port_.write(&cmd, 1)) {
        setError("Failed to send accept 0x10");
        return false;
    }
    escrowState_ = EscrowState::Idle;
    return true;
}

bool Lv77Comm::rejectBill() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint8_t cmd = CMD_REJECT_BILL;
    if (!port_.write(&cmd, 1)) {
        setError("Failed to send reject 0x0F");
        return false;
    }
    escrowState_ = EscrowState::Idle;
    return true;
}

void Lv77Comm::pollLoopThread() {
    const auto pollInterval = std::chrono::milliseconds(pollIntervalMs_);
    int noResponseCount = 0;
    while (pollLoopRunning_) {
        uint8_t resp = 0;
        auto loopStart = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!port_.isOpen()) break;
            uint8_t cmd = CMD_POLL_STATUS;
            port_.write(&cmd, 1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        size_t n = 0;
        if (!port_.read(&resp, 1, n, pollIntervalMs_)) {
            noResponseCount++;
            if (noResponseCount == 10) {
                logging::Logger::getInstance().warn("[LV77] No response to poll (check COM/cable). Slowing poll to 2s.");
            } else if (noResponseCount > 10) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
            std::this_thread::sleep_for(pollInterval);
            continue;
        }
        noResponseCount = 0;
        if (n == 0) continue;

        // 3.2 Escrow: 0x81 수신 → 지폐코드 읽기 → 수락/반환 결정 → 0x02 또는 0x0F 전송 (초과 반환 확실히 동작)
        if (resp == RSP_BILL_VALIDATED) {
            uint8_t billType = 0;
            if (!readByte(billType, 500) || !isBillTypeCode(billType)) {
                logging::Logger::getInstance().warn("[LV77] Escrow: failed to read bill type after 0x81, sending reject");
                std::lock_guard<std::mutex> lock(mutex_);
                uint8_t cmd = CMD_REJECT_BILL;  // 0x0F
                port_.write(&cmd, 1);
                escrowState_ = EscrowState::Idle;
                continue;
            }
            uint32_t amount = billCodeToAmount(billType);
            escrowAmount_ = amount;
            escrowState_ = EscrowState::WaitingAcceptReject;
            bool accept = true;
            if (escrowCallback_) accept = escrowCallback_(amount);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                uint8_t cmd = accept ? CMD_SYNC_ACK : CMD_REJECT_BILL;  // 0x02 or 0x0F
                port_.write(&cmd, 1);
            }
            if (accept) {
                logging::Logger::getInstance().info("[LV77] Escrow accept (0x02): " + std::to_string(amount) + " KRW");
            } else {
                logging::Logger::getInstance().info("[LV77] Escrow reject (0x0F): " + std::to_string(amount) + " KRW");
            }
            escrowState_ = EscrowState::Idle;
            continue;
        }
        if (resp == RSP_STACKING && billStackedCallback_) {
            billStackedCallback_(escrowAmount_);
        } else if (statusCallback_) {
            statusCallback_(resp);
        }
        auto elapsed = std::chrono::steady_clock::now() - loopStart;
        if (elapsed < pollInterval) {
            std::this_thread::sleep_for(pollInterval - elapsed);
        }
    }
}

void Lv77Comm::startPollLoop(uint32_t pollIntervalMs) {
    if (pollLoopRunning_) return;
    pollIntervalMs_ = pollIntervalMs;
    pollLoopRunning_ = true;
    pollLoopThread_ = std::thread(&Lv77Comm::pollLoopThread, this);
    logging::Logger::getInstance().info("[LV77] Poll loop started, interval " + std::to_string(pollIntervalMs) + " ms");
}

void Lv77Comm::stopPollLoop() {
    if (!pollLoopRunning_) return;
    pollLoopRunning_ = false;
    if (pollLoopThread_.joinable()) pollLoopThread_.join();
    logging::Logger::getInstance().info("[LV77] Poll loop stopped");
}

} // namespace lv77
