// tests/test_lv77_client.cpp
// Standalone LV77 (ICT-104U) bill validator test client.
// Usage: test_lv77_client.exe [COM port] [options]
//   --sync   = power-up sync (0x02 -> 0x8F); use only right after device power-on.
//   --8n1    = use 8N1 instead of 8E1 (try if device does not respond).
//   --verbose = print every byte received [RX] 0xXX for debugging.

#include "vendor_adapters/lv77/lv77_comm.h"
#include "vendor_adapters/lv77/lv77_protocol.h"
#include "vendor_adapters/smartro/serial_port.h"
#include <iostream>
#include <string>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::string port = "COM4";
    bool doSync = false;
    bool use8n1 = false;
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--sync") doSync = true;
        else if (arg == "--8n1") use8n1 = true;
        else if (arg == "--verbose" || arg == "-v") verbose = true;
        else if (!arg.empty() && arg[0] != '-') port = arg;
    }

    std::cout << "LV77 Bill Validator Test Client (ICT-104U)" << std::endl;
    std::cout << "Port: " << port << " (9600 " << (use8n1 ? "8N1" : "8E1") << ")" << std::endl;
    if (verbose) std::cout << "Verbose: every RX byte will be printed." << std::endl;

    smartro::SerialPort serial;
    lv77::Lv77Comm comm(serial);

    if (!comm.open(port)) {
        std::cerr << "Failed to open " << port << ": " << comm.getLastError() << std::endl;
        return 1;
    }
    if (use8n1) {
        serial.setParity(0);  // NOPARITY -> 8N1
        std::cout << "Using 8N1 (no parity)." << std::endl;
    }

    if (doSync) {
        std::cout << "Power-up sync (0x02 -> 0x8F, use only right after device power-on)..." << std::endl;
        if (!comm.syncAfterPowerUp(2000)) {
            std::cerr << "Sync failed: " << comm.getLastError() << std::endl;
            comm.close();
            return 1;
        }
        std::cout << "Sync OK." << std::endl;
    } else {
        std::cout << "Skipping sync (device already on). Use --sync if you just powered the device." << std::endl;
    }

    std::cout << "Enable (0x3E)..." << std::endl;
    if (!comm.enable()) {
        std::cerr << "Enable failed: " << comm.getLastError() << std::endl;
        comm.close();
        return 1;
    }

    std::cout << "Polling (0x0C). Insert bill or press Enter to quit." << std::endl;
    std::cout << "(If nothing happens, try: " << port << " --8n1   or   " << port << " --verbose)" << std::endl;

    int timeoutCount = 0;
    while (true) {
        uint8_t resp = 0;
        if (!comm.poll(resp, 800)) {
            ++timeoutCount;
            if (!verbose) {
                std::cout << "." << std::flush;
                if (timeoutCount > 0 && timeoutCount % 30 == 0)
                    std::cout << " [no RX yet - try --8n1 or --verbose]\n" << std::flush;
            }
            continue;
        }
        timeoutCount = 0;

        if (verbose) std::cout << "[RX] 0x" << std::hex << (int)resp << std::dec << std::endl;

        if (resp == lv77::RSP_BILL_VALIDATED) {
            uint8_t billType = 0;
            size_t n = 0;
            if (!serial.read(&billType, 1, n, 1000) || n == 0) {
                std::cout << "[?] No bill type after 0x81" << std::endl;
                continue;
            }
            if (verbose) std::cout << "[RX] 0x" << std::hex << (int)billType << std::dec << " (bill type)" << std::endl;
            if (lv77::isBillTypeCode(billType)) {
                uint32_t amount = lv77::billCodeToAmount(billType);
                std::cout << "[Bill] " << amount << " KRW - accepting (0x10)" << std::endl;
                comm.acceptBill();
            }
            continue;
        }

        if (resp == lv77::RSP_STACKING) {
            std::cout << "[Stacking]" << std::endl;
            continue;
        }

        if (resp == lv77::STATUS_ENABLE) {
            std::cout << "[Status] Enable" << std::endl;
            continue;
        }
        if (resp == lv77::STATUS_INHIBIT) {
            std::cout << "[Status] Inhibit" << std::endl;
            continue;
        }

        std::string statusStr = lv77::statusCodeToString(resp);
        if (statusStr.find("Unknown") == 0)
            std::cout << "[Response] 0x" << std::hex << (int)resp << std::dec << " " << statusStr << std::endl;
        else
            std::cout << "[Status] " << statusStr << std::endl;
    }

    comm.close();
    return 0;
}
