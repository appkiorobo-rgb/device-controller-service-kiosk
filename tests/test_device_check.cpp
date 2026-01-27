// tests/test_device_check.cpp
#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include "vendor_adapters/smartro/serial_port.h"
#include "vendor_adapters/smartro/smartro_comm.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include "logging/logger.h"

using namespace smartro;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [COM_PORT] [TERMINAL_ID] [BAUD_RATE]" << std::endl;
    std::cout << "  COM_PORT: Serial port name (optional, e.g., COM3)" << std::endl;
    std::cout << "           If not specified, will auto-detect and try all available ports" << std::endl;
    std::cout << "  TERMINAL_ID: Terminal ID (default: DEFAULT_TERM)" << std::endl;
    std::cout << "  BAUD_RATE: Baud rate (default: 115200)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << "                    (auto-detect COM port)" << std::endl;
    std::cout << "  " << programName << " COM3                (use COM3)" << std::endl;
    std::cout << "  " << programName << " COM3 TERM001 115200" << std::endl;
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

int main(int argc, char* argv[]) {
    try {
        // 로거 초기화
        logging::Logger::getInstance().initialize("device_check_test.log");
        
        std::string comPort;
        std::string terminalId = "DEFAULT_TERM";
        uint32_t baudRate = 115200;
        
        // 인자 파싱
        int argIndex = 1;
        if (argc > argIndex) {
            std::string firstArg = argv[argIndex];
            // 첫 번째 인자가 숫자로 시작하면 COM 포트로 간주
            if (firstArg.find("COM") == 0 || firstArg.find("com") == 0) {
                comPort = firstArg;
                argIndex++;
            } else {
                // 숫자만 있으면 COM 포트로 간주
                try {
                    int portNum = std::stoi(firstArg);
                    comPort = "COM" + std::to_string(portNum);
                    argIndex++;
                } catch (...) {
                    // COM 포트가 아니면 Terminal ID로 간주
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
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid baud rate: " << argv[argIndex] << std::endl;
                logging::Logger::getInstance().error("Invalid baud rate: " + std::string(argv[argIndex]));
                logging::Logger::getInstance().shutdown();
                std::cout << std::endl << "Press Enter to exit..." << std::endl;
                std::cin.get();
                return 1;
            }
        }
        
        logging::Logger::getInstance().info("=== Device Check Test ===");
        logging::Logger::getInstance().info("Terminal ID: " + terminalId);
        logging::Logger::getInstance().info("Baud Rate: " + std::to_string(baudRate));
        
        // COM 포트 목록 가져오기
        std::vector<std::string> availablePorts;
        if (comPort.empty()) {
            // 저장된 포트 확인
            std::string savedPort = SerialPort::loadWorkingPort();
            if (!savedPort.empty()) {
                std::cout << "Using saved port: " << savedPort << std::endl;
                availablePorts.push_back(savedPort);
                logging::Logger::getInstance().info("Using saved COM Port: " + savedPort);
            } else {
                // 자동 감지
                std::cout << "Auto-detecting COM ports..." << std::endl;
                availablePorts = SerialPort::getAvailablePorts();
                
                if (availablePorts.empty()) {
                    std::cerr << "Error: No COM ports found" << std::endl;
                    logging::Logger::getInstance().error("No COM ports found");
                    logging::Logger::getInstance().shutdown();
                    std::cout << std::endl << "Press Enter to exit..." << std::endl;
                    std::cin.get();
                    return 1;
                }
                
                std::cout << "Found " << availablePorts.size() << " COM port(s): ";
                for (size_t i = 0; i < availablePorts.size(); ++i) {
                    std::cout << availablePorts[i];
                    if (i < availablePorts.size() - 1) std::cout << ", ";
                }
                std::cout << std::endl;
            }
        } else {
            // 지정된 포트만 사용
            availablePorts.push_back(comPort);
            logging::Logger::getInstance().info("Using specified COM Port: " + comPort);
        }
        
        // 각 포트에 대해 시도
        bool success = false;
        DeviceCheckResponse response;
        std::string workingPort;
        
        for (const auto& port : availablePorts) {
            if (availablePorts.size() > 1) {
                std::cout << "Trying " << port << "..." << std::endl;
            }
            logging::Logger::getInstance().debug("Trying COM port: " + port);
            
            SerialPort serialPort;
            if (!serialPort.open(port, baudRate)) {
                if (availablePorts.size() > 1) {
                    std::cout << "  Failed to open " << port << std::endl;
                }
                logging::Logger::getInstance().warn("Failed to open port: " + port);
                continue;
            }
            
            // 통신 객체 생성
            SmartroComm comm(serialPort);
            
            // 장치체크 요청 전송
            if (comm.sendDeviceCheckRequest(terminalId, response)) {
                workingPort = port;
                success = true;
                
                // 성공한 포트 저장
                SerialPort::saveWorkingPort(port);
                
                if (availablePorts.size() > 1) {
                    std::cout << "  Success! Device found on " << port << std::endl;
                }
                logging::Logger::getInstance().info("Device found on port: " + port);
                
                serialPort.close();
                break;
            } else {
                if (availablePorts.size() > 1) {
                    std::cout << "  No response from " << port << std::endl;
                }
                logging::Logger::getInstance().warn("No response from port: " + port);
            }
            
            serialPort.close();
        }
        
        if (success) {
            printDeviceStatus(response);
            std::cout << std::endl << "Device check completed successfully on " << workingPort << "!" << std::endl;
        } else {
            std::cerr << std::endl << "Error: Device check failed on all ports" << std::endl;
            if (!availablePorts.empty()) {
                std::cerr << "Tried ports: ";
                for (size_t i = 0; i < availablePorts.size(); ++i) {
                    std::cerr << availablePorts[i];
                    if (i < availablePorts.size() - 1) std::cerr << ", ";
                }
                std::cerr << std::endl;
            }
        }
        
        logging::Logger::getInstance().info("Test completed");
        logging::Logger::getInstance().shutdown();
        
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
    
    // 로그를 확인할 수 있도록 일시정지
    std::cout << std::endl << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
