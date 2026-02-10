// include/vendor_adapters/lv77/lv77_protocol.h
// ICT-104U / LV77 Bill Validator Protocol (RS232 single-byte commands)
#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#include <cstdint>
#include <string>

namespace lv77 {

// ---- Controller -> Bill Acceptor (Host -> Device) ----
constexpr uint8_t CMD_SYNC_ACK         = 0x02;  // Send within 2 sec after power-up; enables acceptor
constexpr uint8_t CMD_POLL_STATUS      = 0x0C;  // Request status (must poll within 5 sec)
constexpr uint8_t CMD_REJECT_BILL      = 0x0F;  // Reject bill in escrow
constexpr uint8_t CMD_ACCEPT_STACK     = 0x10;  // Accept (stack) bill
constexpr uint8_t CMD_REJECT_STACK     = 0x11;  // Reject (return) bill
constexpr uint8_t CMD_HOLD_ESCROW      = 0x18;  // Hold in escrow until Accept/Reject
constexpr uint8_t CMD_RESET            = 0x30;  // Reset bill acceptor
constexpr uint8_t CMD_ENABLE           = 0x3E;  // Enable bill acceptor
constexpr uint8_t CMD_DISABLE          = 0x5E;  // Disable bill acceptor (94 decimal)
constexpr uint8_t CMD_ESCROW_HOLD      = 0x5A;  // Escrow hold (V0.3)

// ---- Bill Acceptor -> Controller (Device -> Host) ----
constexpr uint8_t RSP_POWER_UP         = 0x80;  // Power supply ON (device sends)
constexpr uint8_t RSP_SYNC_OK          = 0x8F;  // Response to 0x02 within 2 sec
constexpr uint8_t RSP_BILL_VALIDATED   = 0x81;  // Bill in escrow (next byte = bill type 0x40~0x44)
constexpr uint8_t RSP_BILL_TYPE_FIRST  = 0x40;  // First bill type (e.g. 1000 KRW)
constexpr uint8_t RSP_BILL_TYPE_SECOND = 0x41;  // Second
constexpr uint8_t RSP_BILL_TYPE_THIRD  = 0x42;  // Third
constexpr uint8_t RSP_BILL_TYPE_FOURTH = 0x43;  // Fourth
constexpr uint8_t RSP_BILL_TYPE_FIFTH  = 0x44;  // Fifth
constexpr uint8_t RSP_STACKING         = 0x10;  // Stacking (after we sent Accept)
constexpr uint8_t RSP_REJECT           = 0x11;  // Reject (after we sent Reject)

// Poll response status codes
constexpr uint8_t STATUS_RESTART_BA    = 0x20;
constexpr uint8_t STATUS_MOTOR_FAILURE = 0x21;
constexpr uint8_t STATUS_CHECKSUM_ERR  = 0x22;
constexpr uint8_t STATUS_BILL_JAM      = 0x23;
constexpr uint8_t STATUS_BILL_REMOVE   = 0x24;
constexpr uint8_t STATUS_STACKER_OPEN  = 0x25;
constexpr uint8_t STATUS_SENSOR_PROBLEM= 0x27;
constexpr uint8_t STATUS_BILL_FISH     = 0x28;
constexpr uint8_t STATUS_STACKER_PROBLEM = 0x29;
constexpr uint8_t STATUS_BILL_REJECT   = 0x2A;
constexpr uint8_t STATUS_INVALID_CMD   = 0x2B;  // and more 2C..2F reserved
constexpr uint8_t STATUS_ENABLE        = 0x3E;  // Bill acceptor enable status
constexpr uint8_t STATUS_INHIBIT       = 0x5E;  // Bill acceptor inhibit status

// Default KRW amounts for bill types (configurable; LV77 type order is machine-dependent)
inline uint32_t billCodeToAmount(uint8_t code) {
    switch (code) {
        case RSP_BILL_TYPE_FIRST:  return 1000;
        case RSP_BILL_TYPE_SECOND: return 5000;
        case RSP_BILL_TYPE_THIRD:  return 10000;
        case RSP_BILL_TYPE_FOURTH: return 50000;
        case RSP_BILL_TYPE_FIFTH:  return 100000;
        default: return 0;
    }
}

inline bool isBillTypeCode(uint8_t code) {
    return code >= RSP_BILL_TYPE_FIRST && code <= RSP_BILL_TYPE_FIFTH;
}

// Only first three bill types accepted (40H=1000, 41H=5000, 42H=10000 KRW)
inline bool isAcceptedBillType(uint8_t code) {
    return code == RSP_BILL_TYPE_FIRST || code == RSP_BILL_TYPE_SECOND || code == RSP_BILL_TYPE_THIRD;
}

inline std::string statusCodeToString(uint8_t code) {
    switch (code) {
        case STATUS_RESTART_BA:     return "Restart BA";
        case STATUS_MOTOR_FAILURE:  return "Motor Failure";
        case STATUS_CHECKSUM_ERR:   return "Checksum Error";
        case STATUS_BILL_JAM:       return "Bill Jam";
        case STATUS_BILL_REMOVE:    return "Bill Remove";
        case STATUS_STACKER_OPEN:   return "Stacker Open";
        case STATUS_SENSOR_PROBLEM: return "Sensor Problem";
        case STATUS_BILL_FISH:      return "Bill Fish";
        case STATUS_STACKER_PROBLEM: return "Stacker Problem";
        case STATUS_BILL_REJECT:    return "Bill Reject";
        case STATUS_INVALID_CMD:    return "Invalid Command";
        case STATUS_ENABLE:        return "Enable";
        case STATUS_INHIBIT:       return "Inhibit";
        default: {
            const char hex[] = "0123456789ABCDEF";
            return std::string("Unknown(0x") + hex[(code >> 4) & 0x0F] + hex[code & 0x0F] + ")";
        }
    }
}

} // namespace lv77
