// tests/test_integrated.cpp
// Integrated test program - All features selectable via menu
#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <iomanip>
#include "vendor_adapters/smartro/serial_port.h"
#include "vendor_adapters/smartro/smartro_comm.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include "logging/logger.h"

using namespace smartro;

void printMenu() {
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  SMARTRO Payment Terminal Test Menu" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  A - Device Check" << std::endl;
    std::cout << "  B - Payment Approval (Sync)" << std::endl;
    std::cout << "  C - Payment Approval (Async) + Cancel Test" << std::endl;
    std::cout << "  D - Transaction Cancel (거래 취소) - L + C Test" << std::endl;
    std::cout << "  E - Payment Wait" << std::endl;
    std::cout << "  F - Card UID Read" << std::endl;
    std::cout << "  L - Last Approval Response" << std::endl;
    std::cout << "  M - IC Card Check" << std::endl;
    std::cout << "  R - Reset Terminal" << std::endl;
    std::cout << "  S - Screen/Sound Setting" << std::endl;
    std::cout << "  @ - Wait for Event" << std::endl;
    std::cout << "  Q - Quit" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Select option: ";
}

void printDeviceStatus(const DeviceCheckResponse& response) {
    std::cout << std::endl;
    std::cout << "=== Device Check Results ===" << std::endl;
    
    const char* cardStatus = "Unknown";
    switch (response.cardModuleStatus) {
        case 'N': cardStatus = "Not Installed"; break;
        case 'O': cardStatus = "Normal"; break;
        case 'X': cardStatus = "Error"; break;
    }
    std::cout << "Card Module: " << response.cardModuleStatus << " (" << cardStatus << ")" << std::endl;
    
    const char* rfStatus = "Unknown";
    switch (response.rfModuleStatus) {
        case 'O': rfStatus = "Normal"; break;
        case 'X': rfStatus = "Error"; break;
    }
    std::cout << "RF Module: " << response.rfModuleStatus << " (" << rfStatus << ")" << std::endl;
    
    const char* vanStatus = "Unknown";
    switch (response.vanServerStatus) {
        case 'N': vanStatus = "Not Installed"; break;
        case 'O': vanStatus = "Normal"; break;
        case 'X': vanStatus = "Connection Device Error"; break;
        case 'F': vanStatus = "Server Connection Failed"; break;
    }
    std::cout << "VAN Server: " << response.vanServerStatus << " (" << vanStatus << ")" << std::endl;
    
    const char* intStatus = "Unknown";
    switch (response.integrationServerStatus) {
        case 'N': intStatus = "Not Installed"; break;
        case 'O': intStatus = "Normal"; break;
        case 'X': intStatus = "Connection Device Error"; break;
        case 'F': intStatus = "Server Connection Failed"; break;
    }
    std::cout << "Integration Server: " << response.integrationServerStatus << " (" << intStatus << ")" << std::endl;
    std::cout << "===========================" << std::endl;
}

void printCardUidResponse(const CardUidReadResponse& response) {
    std::cout << std::endl;
    std::cout << "=== Card UID Read Response ===" << std::endl;
    
    if (response.uid.empty()) {
        std::cout << "No card detected or UID is empty" << std::endl;
    } else {
        std::cout << "UID Length: " << response.uid.size() << " bytes" << std::endl;
        std::cout << "UID (Hex): ";
        for (size_t i = 0; i < response.uid.size(); ++i) {
            printf("%02X ", response.uid[i]);
        }
        std::cout << std::endl;
    }
    std::cout << "==============================" << std::endl;
}

void printEventResponse(const EventResponse& event) {
    std::cout << std::endl;
    std::cout << "=== Event Received ===" << std::endl;
    
    const char* eventTypeStr = "Unknown";
    switch (event.type) {
        case EventType::MS_CARD_DETECTED:
            eventTypeStr = "MS Card Detected (@M)";
            break;
        case EventType::RF_CARD_DETECTED:
            eventTypeStr = "RF Card Detected (@R)";
            break;
        case EventType::IC_CARD_DETECTED:
            eventTypeStr = "IC Card Detected (@I)";
            break;
        case EventType::IC_CARD_REMOVED:
            eventTypeStr = "IC Card Removed (@O)";
            break;
        case EventType::IC_CARD_FALLBACK:
            eventTypeStr = "IC Card Fallback (@F)";
            break;
        case EventType::UNKNOWN:
            eventTypeStr = "Unknown Event";
            break;
    }
    
    std::cout << "Event Type: " << eventTypeStr << std::endl;
    std::cout << "=====================" << std::endl;
}

bool openSerialPort(SerialPort& serialPort, const std::string& comPort, uint32_t baudRate) {
    if (comPort.empty()) {
        // Check saved port
        std::string savedPort = SerialPort::loadWorkingPort();
        if (!savedPort.empty()) {
            std::cout << "Using saved port: " << savedPort << std::endl;
            if (serialPort.open(savedPort, baudRate)) {
                return true;
            }
        }
        
        // Auto-detect
        std::cout << "Auto-detecting COM ports..." << std::endl;
        auto availablePorts = SerialPort::getAvailablePorts();
        
        if (availablePorts.empty()) {
            std::cerr << "Error: No COM ports found" << std::endl;
            return false;
        }
        
        for (const auto& port : availablePorts) {
            if (serialPort.open(port, baudRate)) {
                SerialPort::saveWorkingPort(port);
                std::cout << "Opened port: " << port << std::endl;
                return true;
            }
        }
        
        return false;
    } else {
        if (serialPort.open(comPort, baudRate)) {
            SerialPort::saveWorkingPort(comPort);
            return true;
        }
        return false;
    }
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        logging::Logger::getInstance().initialize("integrated_test.log");
        
        std::string comPort;
        std::string terminalId = "DEFAULT_TERM";
        uint32_t baudRate = 115200;
        
        // Parse arguments
        int argIndex = 1;
        if (argc > argIndex) {
            std::string firstArg = argv[argIndex];
            if (firstArg.find("COM") == 0 || firstArg.find("com") == 0) {
                comPort = firstArg;
                argIndex++;
            } else {
                try {
                    int portNum = std::stoi(firstArg);
                    comPort = "COM" + std::to_string(portNum);
                    argIndex++;
                } catch (...) {
                    terminalId = firstArg;
                    argIndex++;
                }
            }
        }
        
        if (argc > argIndex) {
            terminalId = argv[argIndex];
            argIndex++;
        }
        
        if (argc > argIndex) {
            try {
                baudRate = std::stoul(argv[argIndex]);
            } catch (...) {
                // Ignore
            }
        }
        
        logging::Logger::getInstance().info("=== Integrated Test Started ===");
        logging::Logger::getInstance().info("Terminal ID: " + terminalId);
        logging::Logger::getInstance().info("Baud Rate: " + std::to_string(baudRate));
        
        // Open Serial Port
        SerialPort serialPort;
        if (!openSerialPort(serialPort, comPort, baudRate)) {
            std::cerr << "Error: Failed to open serial port" << std::endl;
            logging::Logger::getInstance().error("Failed to open serial port");
            logging::Logger::getInstance().shutdown();
            std::cout << std::endl << "Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
        
        // Create communication object
        SmartroComm comm(serialPort);
        
        // Start response receiver thread for async operations
        comm.startResponseReceiver();
        
        // Main loop
        std::string input;
        while (true) {
            printMenu();
            std::getline(std::cin, input);
            
            if (input.empty()) {
                continue;
            }
            
            char choice = std::toupper(input[0]);
            
            switch (choice) {
                case 'A': {
                    std::cout << std::endl << ">>> Device Check Request..." << std::endl;
                    DeviceCheckResponse response;
                    if (comm.sendDeviceCheckRequest(terminalId, response)) {
                        printDeviceStatus(response);
                        std::cout << std::endl << "Device check completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "Device check failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case 'C': {
                    std::cout << std::endl << ">>> Payment Approval Request (Async) + Cancel Test..." << std::endl;
                    std::cout << "This demonstrates async pattern: B request sent, then E (cancel) can be sent immediately" << std::endl;
                    
                    PaymentApprovalRequest request;
                    std::cout << "Amount (KRW): ";
                    std::string input;
                    std::getline(std::cin, input);
                    try {
                        request.amount = std::stoul(input);
                    } catch (...) {
                        request.amount = 1000;
                    }
                    
                    request.transactionType = 1;  // Approval
                    request.tax = 0;
                    request.service = 0;
                    request.installments = 0;
                    request.signatureRequired = 1;
                    
                    // 비동기로 요청 전송 (바로 반환)
                    if (!comm.sendPaymentApprovalRequestAsync(terminalId, request)) {
                        std::cerr << std::endl << "Failed to send payment approval request: " << comm.getLastError() << std::endl;
                        break;
                    }
                    
                    std::cout << std::endl << "Payment approval request sent (async). Waiting for response..." << std::endl;
                    std::cout << "You can now send E (Payment Wait/Cancel) if needed." << std::endl;
                    std::cout << "Press Enter to check for response, or type 'E' to send cancel..." << std::endl;
                    
                    // 응답 폴링 또는 취소 대기
                    bool waiting = true;
                    while (waiting) {
                        std::string userInput;
                        std::getline(std::cin, userInput);
                        
                        if (userInput == "E" || userInput == "e") {
                            // 취소 요청 (E)
                            std::cout << std::endl << ">>> Sending Payment Wait (Cancel) Request..." << std::endl;
                            PaymentWaitResponse cancelResponse;
                            if (comm.sendPaymentWaitRequest(terminalId, cancelResponse)) {
                                std::cout << std::endl << "Cancel request sent successfully!" << std::endl;
                            } else {
                                std::cerr << std::endl << "Cancel request failed: " << comm.getLastError() << std::endl;
                            }
                        }
                        
                        // 응답 확인
                        ResponseData response;
                        if (comm.pollResponse(response, 100)) {  // 100ms 타임아웃
                            if (response.type == ResponseType::PAYMENT_APPROVAL) {
                                std::cout << std::endl << "=== Payment Approval Response Received ===" << std::endl;
                                std::cout << "Data Length: " << response.paymentApproval.data.size() << " bytes" << std::endl;
                                if (!response.paymentApproval.data.empty()) {
                                    std::cout << "Data (Hex): ";
                                    for (size_t i = 0; i < response.paymentApproval.data.size() && i < 64; ++i) {
                                        printf("%02X ", response.paymentApproval.data[i]);
                                    }
                                    if (response.paymentApproval.data.size() > 64) {
                                        std::cout << "...";
                                    }
                                    std::cout << std::endl;
                                }
                                std::cout << "===========================================" << std::endl;
                                waiting = false;
                            } else if (response.type == ResponseType::PAYMENT_WAIT) {
                                std::cout << std::endl << "=== Payment Wait Response Received ===" << std::endl;
                                std::cout << "Data Length: " << response.paymentWait.data.size() << " bytes" << std::endl;
                                std::cout << "===========================================" << std::endl;
                            }
                        }
                        
                        if (userInput.empty() && !waiting) {
                            break;
                        }
                    }
                    break;
                }
                
                case 'B': {
                    std::cout << std::endl << ">>> Payment Approval Request (Sync)..." << std::endl;
                    
                    PaymentApprovalRequest request;
                    std::cout << "Transaction Type (1: Approval, 2: Last Transaction Cancel): ";
                    std::string input;
                    std::getline(std::cin, input);
                    try {
                        request.transactionType = static_cast<uint8_t>(std::stoul(input));
                    } catch (...) {
                        request.transactionType = 1;
                    }
                    
                    std::cout << "Amount (KRW): ";
                    std::getline(std::cin, input);
                    try {
                        request.amount = std::stoul(input);
                    } catch (...) {
                        request.amount = 1000;
                    }
                    
                    std::cout << "Tax (KRW): ";
                    std::getline(std::cin, input);
                    try {
                        request.tax = std::stoul(input);
                    } catch (...) {
                        request.tax = 0;
                    }
                    
                    std::cout << "Service (KRW): ";
                    std::getline(std::cin, input);
                    try {
                        request.service = std::stoul(input);
                    } catch (...) {
                        request.service = 0;
                    }
                    
                    std::cout << "Installments (00: Lump Sum): ";
                    std::getline(std::cin, input);
                    try {
                        request.installments = static_cast<uint8_t>(std::stoul(input));
                    } catch (...) {
                        request.installments = 0;
                    }
                    
                    std::cout << "Signature Required (1: No, 2: Yes): ";
                    std::getline(std::cin, input);
                    try {
                        request.signatureRequired = static_cast<uint8_t>(std::stoul(input));
                    } catch (...) {
                        request.signatureRequired = 1;
                    }
                    
                    PaymentApprovalResponse response;
                    if (comm.sendPaymentApprovalRequest(terminalId, request, response)) {
                        std::cout << std::endl << "=== Payment Approval Response ===" << std::endl;
                        std::cout << "Data Length: " << response.data.size() << " bytes" << std::endl;
                        if (!response.data.empty()) {
                            std::cout << "Data (Hex): ";
                            for (size_t i = 0; i < response.data.size() && i < 64; ++i) {
                                printf("%02X ", response.data[i]);
                            }
                            if (response.data.size() > 64) {
                                std::cout << "...";
                            }
                            std::cout << std::endl;
                        }
                        std::cout << "=================================" << std::endl;
                        std::cout << std::endl << "Payment approval completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "Payment approval failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case 'D': {
                    std::cout << std::endl << ">>> Transaction Cancel Test (L + C)..." << std::endl;
                    std::cout << "Step 1: Getting last approval response (L)..." << std::endl;
                    
                    // Step 1: Get last approval response
                    LastApprovalResponse lastApproval;
                    if (!comm.sendLastApprovalResponseRequest(terminalId, lastApproval)) {
                        std::cerr << std::endl << "Failed to get last approval: " << comm.getLastError() << std::endl;
                        break;
                    }
                    
                    if (lastApproval.data.empty() || lastApproval.data.size() < 157) {
                        std::cerr << std::endl << "Last approval data is empty or invalid (expected 157 bytes, got " 
                                  << lastApproval.data.size() << " bytes)" << std::endl;
                        break;
                    }
                    
                    std::cout << "Last approval response received: " << lastApproval.data.size() << " bytes" << std::endl;
                    
                    // Parse the last approval response to extract cancellation info
                    PaymentApprovalResponse parsedApproval;
                    if (!SmartroProtocol::parsePaymentApprovalResponse(lastApproval.data.data(), lastApproval.data.size(), parsedApproval)) {
                        std::cerr << std::endl << "Failed to parse last approval response" << std::endl;
                        break;
                    }
                    
                    std::cout << std::endl << "=== Parsed Last Approval Info ===" << std::endl;
                    std::cout << "Transaction Type: " << parsedApproval.transactionType << std::endl;
                    std::cout << "Transaction Medium: " << parsedApproval.transactionMedium << std::endl;
                    std::cout << "Card Number: " << parsedApproval.cardNumber << std::endl;
                    std::cout << "Approval Amount: " << parsedApproval.approvalAmount << std::endl;
                    std::cout << "Approval Number: " << parsedApproval.approvalNumber << std::endl;
                    std::cout << "Sales Date: " << parsedApproval.salesDate << std::endl;
                    std::cout << "Sales Time: " << parsedApproval.salesTime << std::endl;
                    std::cout << "Transaction ID: " << parsedApproval.transactionId << std::endl;
                    std::cout << "=====================================" << std::endl;
                    
                    // Step 2: Ask user for cancellation type
                    std::cout << std::endl << "Step 2: Transaction Cancel Request (C)" << std::endl;
                    std::cout << "Cancel Type:" << std::endl;
                    std::cout << "  1 - Request message cancellation (요청전문 취소)" << std::endl;
                    std::cout << "  2 - Last transaction cancellation (마지막 거래 취소)" << std::endl;
                    std::cout << "Select cancel type (1 or 2, default: 2): ";
                    std::string cancelTypeInput;
                    std::getline(std::cin, cancelTypeInput);
                    char cancelType = (cancelTypeInput.empty() || cancelTypeInput[0] != '1') ? '2' : '1';
                    
                    // Ask for amount (default: same as approval amount)
                    std::cout << "Cancellation amount (default: " << parsedApproval.approvalAmount << "): ";
                    std::string amountInput;
                    std::getline(std::cin, amountInput);
                    uint32_t cancelAmount = 0;
                    if (amountInput.empty()) {
                        try {
                            cancelAmount = std::stoul(parsedApproval.approvalAmount);
                        } catch (...) {
                            cancelAmount = 0;
                        }
                    } else {
                        try {
                            cancelAmount = std::stoul(amountInput);
                        } catch (...) {
                            std::cerr << "Invalid amount, using approval amount" << std::endl;
                            try {
                                cancelAmount = std::stoul(parsedApproval.approvalAmount);
                            } catch (...) {
                                cancelAmount = 0;
                            }
                        }
                    }
                    
                    // Build cancellation request
                    TransactionCancelRequest cancelRequest;
                    cancelRequest.cancelType = cancelType;
                    cancelRequest.transactionType = static_cast<uint8_t>(parsedApproval.transactionType - '0');  // Convert char to uint8_t
                    if (cancelRequest.transactionType == 0 || cancelRequest.transactionType > 9) {
                        cancelRequest.transactionType = 1;  // Default to credit approval
                    }
                    cancelRequest.amount = cancelAmount;
                    cancelRequest.tax = 0;
                    try {
                        cancelRequest.tax = std::stoul(parsedApproval.tax);
                    } catch (...) {
                        cancelRequest.tax = 0;
                    }
                    cancelRequest.service = 0;
                    try {
                        cancelRequest.service = std::stoul(parsedApproval.serviceCharge);
                    } catch (...) {
                        cancelRequest.service = 0;
                    }
                    cancelRequest.installments = 0;
                    try {
                        cancelRequest.installments = static_cast<uint8_t>(std::stoi(parsedApproval.installments));
                    } catch (...) {
                        cancelRequest.installments = 0;
                    }
                    cancelRequest.approvalNumber = parsedApproval.approvalNumber;
                    cancelRequest.originalDate = parsedApproval.salesDate;
                    cancelRequest.originalTime = parsedApproval.salesTime;
                    cancelRequest.additionalInfo = "";  // PG cancellation would need 30-digit transaction number
                    
                    std::cout << std::endl << "Sending transaction cancel request..." << std::endl;
                    std::cout << "  Cancel Type: " << cancelType << std::endl;
                    std::cout << "  Amount: " << cancelRequest.amount << std::endl;
                    std::cout << "  Approval Number: " << cancelRequest.approvalNumber << std::endl;
                    std::cout << "  Original Date: " << cancelRequest.originalDate << std::endl;
                    std::cout << "  Original Time: " << cancelRequest.originalTime << std::endl;
                    
                    // Send cancellation request
                    TransactionCancelResponse cancelResponse;
                    if (comm.sendTransactionCancelRequest(terminalId, cancelRequest, cancelResponse)) {
                        std::cout << std::endl << "=== Transaction Cancel Response ===" << std::endl;
                        std::cout << "Transaction Type: " << cancelResponse.transactionType << std::endl;
                        std::cout << "Transaction Medium: " << cancelResponse.transactionMedium << std::endl;
                        std::cout << "Card Number: " << cancelResponse.cardNumber << std::endl;
                        std::cout << "Approval Amount: " << cancelResponse.approvalAmount << std::endl;
                        std::cout << "Approval Number: " << cancelResponse.approvalNumber << std::endl;
                        std::cout << "Sales Date: " << cancelResponse.salesDate << std::endl;
                        std::cout << "Sales Time: " << cancelResponse.salesTime << std::endl;
                        std::cout << "Transaction ID: " << cancelResponse.transactionId << std::endl;
                        std::cout << "Status: " << (cancelResponse.isSuccess() ? "SUCCESS" : "REJECTED") << std::endl;
                        if (cancelResponse.isRejected()) {
                            std::cout << "Rejection Info: " << cancelResponse.rejectionInfo << std::endl;
                        }
                        std::cout << "===========================================" << std::endl;
                        std::cout << std::endl << "Transaction cancel completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "Transaction cancel failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case 'E': {
                    std::cout << std::endl << ">>> Payment Wait Request..." << std::endl;
                    PaymentWaitResponse response;
                    if (comm.sendPaymentWaitRequest(terminalId, response)) {
                        std::cout << std::endl << "=== Payment Wait Response ===" << std::endl;
                        std::cout << "Data Length: " << response.data.size() << " bytes" << std::endl;
                        std::cout << "=============================" << std::endl;
                        std::cout << std::endl << "Payment wait completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "Payment wait failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case 'F': {
                    std::cout << std::endl << ">>> Card UID Read Request..." << std::endl;
                    CardUidReadResponse response;
                    if (comm.sendCardUidReadRequest(terminalId, response)) {
                        printCardUidResponse(response);
                        std::cout << std::endl << "Card UID read completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "Card UID read failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case 'L': {
                    std::cout << std::endl << ">>> Last Approval Response Request..." << std::endl;
                    LastApprovalResponse response;
                    if (comm.sendLastApprovalResponseRequest(terminalId, response)) {
                        std::cout << std::endl << "=== Last Approval Response ===" << std::endl;
                        std::cout << "Data Length: " << response.data.size() << " bytes" << std::endl;
                        
                        if (!response.data.empty() && response.data.size() >= 157) {
                            // Parse and display detailed information
                            PaymentApprovalResponse parsed;
                            if (SmartroProtocol::parsePaymentApprovalResponse(response.data.data(), response.data.size(), parsed)) {
                                std::cout << "Transaction Type: " << parsed.transactionType << std::endl;
                                std::cout << "Transaction Medium: " << parsed.transactionMedium << std::endl;
                                std::cout << "Card Number: " << parsed.cardNumber << std::endl;
                                std::cout << "Approval Amount: " << parsed.approvalAmount << std::endl;
                                std::cout << "Tax: " << parsed.tax << std::endl;
                                std::cout << "Service Charge: " << parsed.serviceCharge << std::endl;
                                std::cout << "Installments: " << parsed.installments << std::endl;
                                std::cout << "Approval Number: " << parsed.approvalNumber << std::endl;
                                std::cout << "Sales Date: " << parsed.salesDate << std::endl;
                                std::cout << "Sales Time: " << parsed.salesTime << std::endl;
                                std::cout << "Transaction ID: " << parsed.transactionId << std::endl;
                                std::cout << "Merchant Number: " << parsed.merchantNumber << std::endl;
                                std::cout << "Terminal Number: " << parsed.terminalNumber << std::endl;
                                std::cout << "Issuer: " << parsed.issuer << std::endl;
                                std::cout << "Acquirer: " << parsed.acquirer << std::endl;
                                std::cout << "Status: " << (parsed.isSuccess() ? "SUCCESS" : "REJECTED") << std::endl;
                            } else {
                                std::cout << "Data (Hex): ";
                                for (size_t i = 0; i < response.data.size() && i < 64; ++i) {
                                    printf("%02X ", response.data[i]);
                                }
                                if (response.data.size() > 64) {
                                    std::cout << "...";
                                }
                                std::cout << std::endl;
                            }
                        } else {
                            std::cout << "Data (Hex): ";
                            for (size_t i = 0; i < response.data.size() && i < 64; ++i) {
                                printf("%02X ", response.data[i]);
                            }
                            if (response.data.size() > 64) {
                                std::cout << "...";
                            }
                            std::cout << std::endl;
                        }
                        std::cout << "===================================" << std::endl;
                        std::cout << std::endl << "Last approval response completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "Last approval response failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case 'M': {
                    std::cout << std::endl << ">>> IC Card Check Request..." << std::endl;
                    IcCardCheckResponse response;
                    if (comm.sendIcCardCheckRequest(terminalId, response)) {
                        std::cout << std::endl << "=== IC Card Check Response ===" << std::endl;
                        const char* statusStr = "Unknown";
                        if (response.cardStatus == 'O') {
                            statusStr = "IC Card Inserted";
                        } else if (response.cardStatus == 'X') {
                            statusStr = "No IC Card";
                        }
                        std::cout << "Card Status: " << response.cardStatus << " (" << statusStr << ")" << std::endl;
                        std::cout << "===================================" << std::endl;
                        std::cout << std::endl << "IC card check completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "IC card check failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case 'R': {
                    std::cout << std::endl << ">>> Reset Terminal Request..." << std::endl;
                    if (comm.sendResetRequest(terminalId)) {
                        std::cout << std::endl << "Reset completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "Reset failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case 'S': {
                    std::cout << std::endl << ">>> Screen/Sound Setting Request..." << std::endl;
                    
                    ScreenSoundSettingRequest request;
                    std::cout << "Screen Brightness (0-9): ";
                    std::string input;
                    std::getline(std::cin, input);
                    try {
                        uint32_t val = std::stoul(input);
                        request.screenBrightness = (val > 9) ? 9 : static_cast<uint8_t>(val);
                    } catch (...) {
                        request.screenBrightness = 5;
                    }
                    
                    std::cout << "Sound Volume (0-9): ";
                    std::getline(std::cin, input);
                    try {
                        uint32_t val = std::stoul(input);
                        request.soundVolume = (val > 9) ? 9 : static_cast<uint8_t>(val);
                    } catch (...) {
                        request.soundVolume = 5;
                    }
                    
                    std::cout << "Touch Sound Volume (0-9): ";
                    std::getline(std::cin, input);
                    try {
                        uint32_t val = std::stoul(input);
                        request.touchSoundVolume = (val > 9) ? 9 : static_cast<uint8_t>(val);
                    } catch (...) {
                        request.touchSoundVolume = 5;
                    }
                    
                    ScreenSoundSettingResponse response;
                    if (comm.sendScreenSoundSettingRequest(terminalId, request, response)) {
                        std::cout << std::endl << "=== Screen/Sound Setting Response ===" << std::endl;
                        std::cout << "Screen Brightness: " << static_cast<int>(response.screenBrightness) << std::endl;
                        std::cout << "Sound Volume: " << static_cast<int>(response.soundVolume) << std::endl;
                        std::cout << "Touch Sound Volume: " << static_cast<int>(response.touchSoundVolume) << std::endl;
                        std::cout << "===========================================" << std::endl;
                        std::cout << std::endl << "Screen/sound setting completed successfully!" << std::endl;
                    } else {
                        std::cerr << std::endl << "Screen/sound setting failed: " << comm.getLastError() << std::endl;
                    }
                    break;
                }
                
                case '@': {
                    std::cout << std::endl << ">>> Waiting for Event..." << std::endl;
                    std::cout << "Enter timeout in milliseconds (0 for infinite): ";
                    std::string input;
                    std::getline(std::cin, input);
                    uint32_t timeoutMs = 0;
                    try {
                        timeoutMs = std::stoul(input);
                    } catch (...) {
                        timeoutMs = 0;
                    }
                    
                    int eventCount = 0;
                    while (true) {
                        EventResponse event;
                        if (comm.waitForEvent(event, timeoutMs)) {
                            eventCount++;
                            printEventResponse(event);
                            std::cout << std::endl << "Event #" << eventCount << " received!" << std::endl;
                            
                            if (timeoutMs > 0) {
                                break;  // If timeout is set, try only once
                            }
                        } else {
                            if (timeoutMs > 0) {
                                std::cout << std::endl << "Timeout waiting for event" << std::endl;
                            } else {
                                std::cerr << std::endl << "Error waiting for event: " << comm.getLastError() << std::endl;
                            }
                            break;
                        }
                    }
                    break;
                }
                
                case 'Q': {
                    std::cout << std::endl << "Exiting..." << std::endl;
                    comm.stopResponseReceiver();
                    serialPort.close();
                    logging::Logger::getInstance().info("Test completed");
                    logging::Logger::getInstance().shutdown();
                    return 0;
                }
                
                default:
                    std::cout << std::endl << "Invalid option. Please try again." << std::endl;
                    break;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << std::endl << "Fatal error: " << e.what() << std::endl;
        logging::Logger::getInstance().error("Fatal exception: " + std::string(e.what()));
        logging::Logger::getInstance().shutdown();
        std::cout << std::endl << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << std::endl << "Unknown fatal error occurred" << std::endl;
        logging::Logger::getInstance().error("Unknown fatal exception");
        logging::Logger::getInstance().shutdown();
        std::cout << std::endl << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    
    return 0;
}
