// src/vendor_adapters/smartro/smartro_protocol.cpp
// logger.h???????? include??? Windows SDK ?? ???
#include "logging/logger.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include <ctime>
#include <algorithm>
#include <cstring>

namespace smartro {

std::vector<uint8_t> SmartroProtocol::createDeviceCheckRequest(const std::string& terminalId) {
    std::vector<uint8_t> packet;
    packet.reserve(MIN_PACKET_SIZE);
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'A'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_DEVICE_CHECK));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length: 0x0000 (Little Endian, USHORT)
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX???ETX???)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created device check request packet: " + 
                                        std::to_string(packet.size()) + " bytes");
    
    return packet;
}

std::vector<uint8_t> SmartroProtocol::createPaymentWaitRequest(const std::string& terminalId) {
    std::vector<uint8_t> packet;
    packet.reserve(MIN_PACKET_SIZE);
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'E'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_PAYMENT_WAIT));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length: 0x0000 (Little Endian, USHORT) - ?????????? ???????????? 0
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX???ETX???)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created payment wait request packet: " + 
                                        std::to_string(packet.size()) + " bytes");
    
    return packet;
}

std::vector<uint8_t> SmartroProtocol::createCardUidReadRequest(const std::string& terminalId) {
    std::vector<uint8_t> packet;
    packet.reserve(MIN_PACKET_SIZE);
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'F'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_CARD_UID_READ));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length: 0x0000 (Little Endian, USHORT) - ?????????? ???????????? 0
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX???ETX???)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created card UID read request packet: " + 
                                        std::to_string(packet.size()) + " bytes");
    
    return packet;
}

std::vector<uint8_t> SmartroProtocol::createResetRequest(const std::string& terminalId) {
    std::vector<uint8_t> packet;
    packet.reserve(MIN_PACKET_SIZE);
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'R'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_RESET));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length: 0x0000 (Little Endian, USHORT) - ?????????? ???????????? 0
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX???ETX???)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created reset request packet: " + 
                                        std::to_string(packet.size()) + " bytes");
    
    return packet;
}

std::vector<uint8_t> SmartroProtocol::createPaymentApprovalRequest(const std::string& terminalId,
                                                                   const PaymentApprovalRequest& request) {
    std::vector<uint8_t> packet;
    packet.reserve(HEADER_SIZE + 30 + TAIL_SIZE);  // Data Length = 30
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'B'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_PAYMENT_APPROVAL));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length: 0x001E (30 bytes, Little Endian, USHORT)
    packet.push_back(0x1E);
    packet.push_back(0x00);
    
    // Data (30 bytes)
    // ?????? (1 byte)
    packet.push_back(request.transactionType);
    
    // ????? (10 bytes, ??????? ??? ?? 0 ???)
    std::string amountStr = std::to_string(request.amount);
    for (size_t i = 0; i < 10; ++i) {
        if (i < 10 - amountStr.length()) {
            packet.push_back('0');
        } else {
            packet.push_back(static_cast<uint8_t>(amountStr[i - (10 - amountStr.length())]));
        }
    }
    
    // ??? (8 bytes, ??????? ??? ?? 0 ???)
    std::string taxStr = std::to_string(request.tax);
    for (size_t i = 0; i < 8; ++i) {
        if (i < 8 - taxStr.length()) {
            packet.push_back('0');
        } else {
            packet.push_back(static_cast<uint8_t>(taxStr[i - (8 - taxStr.length())]));
        }
    }
    
    // ????(8 bytes, ??????? ??? ?? 0 ???)
    std::string serviceStr = std::to_string(request.service);
    for (size_t i = 0; i < 8; ++i) {
        if (i < 8 - serviceStr.length()) {
            packet.push_back('0');
        } else {
            packet.push_back(static_cast<uint8_t>(serviceStr[i - (8 - serviceStr.length())]));
        }
    }
    
    // ?????? (2 bytes, ??????? ??? ?? 0 ???)
    std::string installmentsStr = std::to_string(request.installments);
    for (size_t i = 0; i < 2; ++i) {
        if (i < 2 - installmentsStr.length()) {
            packet.push_back('0');
        } else {
            packet.push_back(static_cast<uint8_t>(installmentsStr[i - (2 - installmentsStr.length())]));
        }
    }
    
    // ??????? (1 byte)
    packet.push_back(request.signatureRequired);
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX???ETX???)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created payment approval request packet: " + 
                                        std::to_string(packet.size()) + " bytes");
    
    return packet;
}

std::vector<uint8_t> SmartroProtocol::createTransactionCancelRequest(const std::string& terminalId,
                                                                     const TransactionCancelRequest& request) {
    // Calculate data length: 57 bytes (base) + 2 bytes (additional info length) + N bytes (additional info)
    size_t additionalInfoLength = request.additionalInfo.length();
    size_t dataLength = 57;
    if (additionalInfoLength > 0) {
        dataLength += 2 + additionalInfoLength;
    }
    
    std::vector<uint8_t> packet;
    packet.reserve(HEADER_SIZE + dataLength + TAIL_SIZE);
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'C'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_TRANSACTION_CANCEL));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length (Little Endian, USHORT)
    packet.push_back(static_cast<uint8_t>(dataLength & 0xFF));
    packet.push_back(static_cast<uint8_t>((dataLength >> 8) & 0xFF));
    
    // Data
    // ?????? (1 byte)
    packet.push_back(static_cast<uint8_t>(request.cancelType));
    
    // ?????? (1 byte)
    packet.push_back(request.transactionType);
    
    // ???? (10 bytes, ?? ?? ?? 0 ??)
    std::string amountStr = std::to_string(request.amount);
    for (size_t i = 0; i < 10; ++i) {
        if (i < 10 - amountStr.length()) {
            packet.push_back('0');
        } else {
            packet.push_back(static_cast<uint8_t>(amountStr[i - (10 - amountStr.length())]));
        }
    }
    
    // ?? (8 bytes, ?? ?? ?? 0 ??)
    std::string taxStr = std::to_string(request.tax);
    for (size_t i = 0; i < 8; ++i) {
        if (i < 8 - taxStr.length()) {
            packet.push_back('0');
        } else {
            packet.push_back(static_cast<uint8_t>(taxStr[i - (8 - taxStr.length())]));
        }
    }
    
    // ??? (8 bytes, ?? ?? ?? 0 ??)
    std::string serviceStr = std::to_string(request.service);
    for (size_t i = 0; i < 8; ++i) {
        if (i < 8 - serviceStr.length()) {
            packet.push_back('0');
        } else {
            packet.push_back(static_cast<uint8_t>(serviceStr[i - (8 - serviceStr.length())]));
        }
    }
    
    // ???? (2 bytes, ?? ?? ?? 0 ??)
    std::string installmentsStr = std::to_string(request.installments);
    for (size_t i = 0; i < 2; ++i) {
        if (i < 2 - installmentsStr.length()) {
            packet.push_back('0');
        } else {
            packet.push_back(static_cast<uint8_t>(installmentsStr[i - (2 - installmentsStr.length())]));
        }
    }
    
    // ???? (12 bytes, ?? ?? ?? ?? ??)
    std::string approvalNumber = request.approvalNumber;
    approvalNumber.resize(12, ' ');
    for (size_t i = 0; i < 12; ++i) {
        packet.push_back(static_cast<uint8_t>(approvalNumber[i]));
    }
    
    // ????? (8 bytes, YYYYMMDD)
    std::string originalDate = request.originalDate;
    originalDate.resize(8, '0');
    for (size_t i = 0; i < 8; ++i) {
        packet.push_back(static_cast<uint8_t>(originalDate[i]));
    }
    
    // ????? (6 bytes, hhmmss)
    std::string originalTime = request.originalTime;
    originalTime.resize(6, '0');
    for (size_t i = 0; i < 6; ++i) {
        packet.push_back(static_cast<uint8_t>(originalTime[i]));
    }
    
    // ???? ?? ? ???? (????? ?? ??)
    if (additionalInfoLength > 0) {
        // ???? ?? (2 bytes, ?? ?? ?? 0 ??)
        std::string infoLengthStr = std::to_string(additionalInfoLength);
        for (size_t i = 0; i < 2; ++i) {
            if (i < 2 - infoLengthStr.length()) {
                packet.push_back('0');
            } else {
                packet.push_back(static_cast<uint8_t>(infoLengthStr[i - (2 - infoLengthStr.length())]));
            }
        }
        
        // ???? (N bytes)
        for (size_t i = 0; i < additionalInfoLength; ++i) {
            packet.push_back(static_cast<uint8_t>(request.additionalInfo[i]));
        }
    }
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX??ETX??)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created transaction cancel request packet: " + 
                                        std::to_string(packet.size()) + " bytes, data length: " + 
                                        std::to_string(dataLength));
    
    return packet;
}

std::vector<uint8_t> SmartroProtocol::createLastApprovalResponseRequest(const std::string& terminalId) {
    std::vector<uint8_t> packet;
    packet.reserve(MIN_PACKET_SIZE);
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'L'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_LAST_APPROVAL_RESPONSE));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length: 0x0000 (Little Endian, USHORT)
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX???ETX???)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created last approval response request packet: " + 
                                        std::to_string(packet.size()) + " bytes");
    
    return packet;
}

std::vector<uint8_t> SmartroProtocol::createScreenSoundSettingRequest(const std::string& terminalId,
                                                                      const ScreenSoundSettingRequest& request) {
    std::vector<uint8_t> packet;
    packet.reserve(HEADER_SIZE + 3 + TAIL_SIZE);  // Data Length = 3
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'S'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_SCREEN_SOUND_SETTING));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length: 0x0003 (3 bytes, Little Endian, USHORT)
    packet.push_back(0x03);
    packet.push_back(0x00);
    
    // Data (3 bytes)
    // ????? (0-9, CHAR)
    uint8_t brightness = (request.screenBrightness > 9) ? '9' : ('0' + request.screenBrightness);
    packet.push_back(brightness);
    
    // ????? (0-9, CHAR)
    uint8_t volume = (request.soundVolume > 9) ? '9' : ('0' + request.soundVolume);
    packet.push_back(volume);
    
    // ????????(0-9, CHAR)
    uint8_t touchVolume = (request.touchSoundVolume > 9) ? '9' : ('0' + request.touchSoundVolume);
    packet.push_back(touchVolume);
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX???ETX???)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created screen/sound setting request packet: " + 
                                        std::to_string(packet.size()) + " bytes");
    
    return packet;
}

std::vector<uint8_t> SmartroProtocol::createIcCardCheckRequest(const std::string& terminalId) {
    std::vector<uint8_t> packet;
    packet.reserve(MIN_PACKET_SIZE);
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code: 'M'
    packet.push_back(static_cast<uint8_t>(JOB_CODE_IC_CARD_CHECK));
    
    // Response Code: 0x00
    packet.push_back(0x00);
    
    // Data Length: 0x0000 (Little Endian, USHORT)
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // ETX
    packet.push_back(ETX);
    
    // BCC ?? (STX???ETX???)
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    logging::Logger::getInstance().debug("Created IC card check request packet: " + 
                                        std::to_string(packet.size()) + " bytes");
    
    return packet;
}

bool SmartroProtocol::parsePacket(const uint8_t* data, size_t length, 
                                 std::vector<uint8_t>& header, 
                                 std::vector<uint8_t>& payload) {
    if (!data || length < MIN_PACKET_SIZE) {
        logging::Logger::getInstance().error("Packet too short: " + std::to_string(length) + " bytes");
        return false;
    }
    
    // STX ???
    if (data[0] != STX) {
        logging::Logger::getInstance().error("Invalid STX: 0x" + 
                                            std::to_string(static_cast<int>(data[0])));
        return false;
    }
    
    // Header ?? (35 bytes)
    header.assign(data, data + HEADER_SIZE);
    
    // Data Length ??
    uint16_t dataLength = extractDataLength(header.data());
    
    // ??? ??? ??? ???
    size_t expectedPacketSize = HEADER_SIZE + dataLength + TAIL_SIZE;
    if (length < expectedPacketSize) {
        logging::Logger::getInstance().error("Packet size mismatch. Expected: " + 
                                            std::to_string(expectedPacketSize) + 
                                            ", Got: " + std::to_string(length));
        return false;
    }
    
    // ETX ???
    size_t etxIndex = HEADER_SIZE + dataLength;
    if (data[etxIndex] != ETX) {
        logging::Logger::getInstance().error("Invalid ETX at index " + 
                                            std::to_string(etxIndex));
        return false;
    }
    
    // BCC ???
    if (!verifyBCC(data, expectedPacketSize)) {
        logging::Logger::getInstance().error("BCC verification failed");
        return false;
    }
    
    // Payload ??
    if (dataLength > 0) {
        payload.assign(data + HEADER_SIZE, data + HEADER_SIZE + dataLength);
    } else {
        payload.clear();
    }
    
    logging::Logger::getInstance().debug("Packet parsed successfully. Data length: " + 
                                        std::to_string(dataLength));
    
    return true;
}

uint8_t SmartroProtocol::calculateBCC(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }
    
    uint8_t bcc = 0;
    for (size_t i = 0; i < length; ++i) {
        bcc ^= data[i];
    }
    
    return bcc;
}

bool SmartroProtocol::verifyBCC(const uint8_t* packet, size_t packetLength) {
    if (!packet || packetLength < TAIL_SIZE) {
        return false;
    }
    
    // ETX ??? (BCC ?? ??
    size_t etxIndex = packetLength - TAIL_SIZE;  // ETX ???
    
    // ?????BCC
    uint8_t receivedBCC = packet[packetLength - 1];
    
    // STX ??? (?????0???ETX???) - SMARTRO ?????? ???
    uint8_t calculatedBCC = calculateBCC(packet, etxIndex + 1);
    
    if (calculatedBCC == receivedBCC) {
        logging::Logger::getInstance().debug("BCC verified (STX included)");
        return true;
    }
    
    logging::Logger::getInstance().warn("BCC mismatch. Calculated: 0x" + 
                                       std::to_string(static_cast<int>(calculatedBCC)) + 
                                       ", Received: 0x" + 
                                       std::to_string(static_cast<int>(receivedBCC)));
    
    return false;
}

bool SmartroProtocol::parseDeviceCheckResponse(const uint8_t* data, size_t length, 
                                               DeviceCheckResponse& response) {
    if (!data || length < 4) {
        logging::Logger::getInstance().error("Device check response too short: " + 
                                            std::to_string(length) + " bytes");
        return false;
    }
    
    response.cardModuleStatus = static_cast<char>(data[0]);
    response.rfModuleStatus = static_cast<char>(data[1]);
    response.vanServerStatus = static_cast<char>(data[2]);
    response.integrationServerStatus = static_cast<char>(data[3]);
    
    // ??? ????INFO????
    std::string statusStr = std::string(1, response.cardModuleStatus) + "/" +
                           std::string(1, response.rfModuleStatus) + "/" +
                           std::string(1, response.vanServerStatus) + "/" +
                           std::string(1, response.integrationServerStatus);
    logging::Logger::getInstance().info("Device check response: " + statusStr);
    
    return true;
}

bool SmartroProtocol::parsePaymentWaitResponse(const uint8_t* data, size_t length, 
                                              PaymentWaitResponse& response) {
    // ?????? ?????E ????? Data Length? 0?????? length? 0??? ???
    if (length == 0) {
        response.data.clear();
        logging::Logger::getInstance().info("Payment wait response received: 0 bytes (no data, as per protocol)");
        return true;
    }
    
    // length? 0???????data? null??? ???
    if (!data) {
        logging::Logger::getInstance().error("Payment wait response data is null but length is " + 
                                            std::to_string(length));
        return false;
    }
    
    // ??? ?????????(??????????????? ???)
    response.data.assign(data, data + length);
    
    logging::Logger::getInstance().info("Payment wait response received: " + 
                                       std::to_string(length) + " bytes");
    
    if (length > 0) {
        std::string hexDump;
        for (size_t i = 0; i < length && i < 32; ++i) {  // ??? 32????? ???
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X ", data[i]);
            hexDump += hex;
        }
        if (length > 32) {
            hexDump += "...";
        }
        logging::Logger::getInstance().debug("Response data: " + hexDump);
    }
    
    return true;
}

bool SmartroProtocol::parseCardUidReadResponse(const uint8_t* data, size_t length, 
                                               CardUidReadResponse& response) {
    // ?? UID??????????4-8 ???????? ?????????? ?????????
    // length? 0??? ??? ??????? ???? ??????
    if (length == 0) {
        response.uid.clear();
        logging::Logger::getInstance().info("Card UID read response: No card detected (0 bytes)");
        return true;  // ??? ??? ?? ??? ??????????
    }
    
    // length? 0???????data? null??? ???
    if (!data) {
        logging::Logger::getInstance().error("Card UID read response data is null but length is " + 
                                            std::to_string(length));
        return false;
    }
    
    // UID ????
    response.uid.assign(data, data + length);
    
    // UID??16?? ????? ???????? ??
    std::string uidHex;
    for (size_t i = 0; i < length; ++i) {
        char hex[4];
        std::snprintf(hex, sizeof(hex), "%02X", data[i]);
        uidHex += hex;
        if (i < length - 1) {
            uidHex += " ";
        }
    }
    
    logging::Logger::getInstance().info("Card UID read response: " + std::to_string(length) + 
                                       " bytes, UID: " + uidHex);
    
    return true;
}

bool SmartroProtocol::parseEventResponse(const uint8_t* data, size_t length, 
                                        EventResponse& response) {
    // ?????? ?? 1????(????????? ??????????
    if (!data || length < 1) {
        logging::Logger::getInstance().error("Event response too short: " + 
                                            std::to_string(length) + " bytes");
        return false;
    }
    
    // ???? ?????? ?????????
    char eventType = static_cast<char>(data[0]);
    
    switch (eventType) {
        case 'M':
            response.type = EventType::MS_CARD_DETECTED;
            logging::Logger::getInstance().info("Event: MS Card Detected (@M)");
            break;
        case 'R':
            response.type = EventType::RF_CARD_DETECTED;
            logging::Logger::getInstance().info("Event: RF Card Detected (@R)");
            break;
        case 'I':
            response.type = EventType::IC_CARD_DETECTED;
            logging::Logger::getInstance().info("Event: IC Card Detected (@I)");
            break;
        case 'O':
            response.type = EventType::IC_CARD_REMOVED;
            logging::Logger::getInstance().info("Event: IC Card Removed (@O)");
            break;
        case 'F':
            response.type = EventType::IC_CARD_FALLBACK;
            logging::Logger::getInstance().info("Event: IC Card Fallback (@F)");
            break;
        default:
            response.type = EventType::UNKNOWN;
            logging::Logger::getInstance().warn("Event: Unknown event type: " + 
                                              std::string(1, eventType));
            break;
    }
    
    // ???? ?????????
    if (length > 1) {
        response.data.assign(data + 1, data + length);
        
        std::string hexDump;
        for (size_t i = 1; i < length && i < 33; ++i) {  // ??? 32????? ???
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X ", data[i]);
            hexDump += hex;
        }
        if (length > 33) {
            hexDump += "...";
        }
        logging::Logger::getInstance().debug("Event data: " + hexDump);
    } else {
        response.data.clear();
    }
    
    return true;
}

bool SmartroProtocol::parsePaymentApprovalResponse(const uint8_t* data, size_t length, 
                                                  PaymentApprovalResponse& response) {
    // ????? ????? 157 bytes (?? ??)
    if (!data) {
        logging::Logger::getInstance().error("Payment approval response data is null");
        return false;
    }
    
    if (length < 157) {
        logging::Logger::getInstance().error("Payment approval response too short: " + 
                                            std::to_string(length) + " bytes, expected 157");
        return false;
    }
    
    // ??? ?????????
    response.data.assign(data, data + length);
    
    size_t offset = 0;
    
    // ?????? (1 byte)
    response.transactionType = static_cast<char>(data[offset++]);
    
    // ???? (1 byte)
    response.transactionMedium = static_cast<char>(data[offset++]);
    
    // ???? (20 bytes)
    response.cardNumber.assign(reinterpret_cast<const char*>(data + offset), 20);
    offset += 20;
    
    // ?????/???? (10 bytes)
    response.approvalAmount.assign(reinterpret_cast<const char*>(data + offset), 10);
    offset += 10;
    
    // ??? (8 bytes)
    response.tax.assign(reinterpret_cast<const char*>(data + offset), 8);
    offset += 8;
    
    // ????(8 bytes)
    response.serviceCharge.assign(reinterpret_cast<const char*>(data + offset), 8);
    offset += 8;
    
    // ?????? (2 bytes)
    response.installments.assign(reinterpret_cast<const char*>(data + offset), 2);
    offset += 2;
    
    // ?????/????? ??? (12 bytes)
    response.approvalNumber.assign(reinterpret_cast<const char*>(data + offset), 12);
    offset += 12;
    
    // ????? (8 bytes, YYYYMMDD)
    response.salesDate.assign(reinterpret_cast<const char*>(data + offset), 8);
    offset += 8;
    
    // ????? (6 bytes, hhmmss)
    response.salesTime.assign(reinterpret_cast<const char*>(data + offset), 6);
    offset += 6;
    
    // ?????? (12 bytes)
    response.transactionId.assign(reinterpret_cast<const char*>(data + offset), 12);
    offset += 12;
    
    // ????? (15 bytes)
    response.merchantNumber.assign(reinterpret_cast<const char*>(data + offset), 15);
    offset += 15;
    
    // ???????(14 bytes)
    response.terminalNumber.assign(reinterpret_cast<const char*>(data + offset), 14);
    offset += 14;
    
    // ????(20 bytes)
    response.issuer.assign(reinterpret_cast<const char*>(data + offset), 20);
    offset += 20;
    
    // ???? ??? (20 bytes, ????????)
    response.rejectionInfo.assign(reinterpret_cast<const char*>(data + offset), 20);
    offset += 20;
    
    // ????(20 bytes)
    response.acquirer.assign(reinterpret_cast<const char*>(data + offset), 20);
    offset += 20;
    
    // ??? ?? ?? ??
    std::string statusStr = response.isRejected() ? "FAILED (Transaction Rejected)" : "SUCCESS";
    
    std::string transactionTypeStr;
    switch (response.transactionType) {
        case '1': transactionTypeStr = "Credit Approval"; break;
        case '2': transactionTypeStr = "Cash Receipt"; break;
        case '3': transactionTypeStr = "Prepaid Card"; break;
        case '4': transactionTypeStr = "Zero Pay"; break;
        case '5': transactionTypeStr = "Kakao Pay (Mini)"; break;
        case '6': transactionTypeStr = "Kakao Pay (Credit)"; break;
        case 'X': 
        case 'x': 
            transactionTypeStr = "Transaction Rejected"; 
            break;
        default: 
            transactionTypeStr = "Unknown(" + std::string(1, response.transactionType) + ")"; 
            break;
    }
    
    std::string mediumStr;
    switch (response.transactionMedium) {
        case '1': mediumStr = "IC"; break;
        case '2': mediumStr = "MS"; break;
        case '3': mediumStr = "RF"; break;
        case '4': mediumStr = "QR"; break;
        case '5': mediumStr = "KEYIN"; break;
        default: 
            mediumStr = "Unknown(" + std::string(1, response.transactionMedium) + ")"; 
            break;
    }
    
    logging::Logger::getInstance().info("Payment approval response parsed:");
    logging::Logger::getInstance().info("  Status: " + statusStr);
    logging::Logger::getInstance().info("  Transaction Type: " + transactionTypeStr);
    logging::Logger::getInstance().info("  Transaction Medium: " + mediumStr);
    logging::Logger::getInstance().info("  Card Number: " + response.cardNumber);
    logging::Logger::getInstance().info("  Approval Amount: " + response.approvalAmount);
    logging::Logger::getInstance().info("  Tax: " + response.tax);
    logging::Logger::getInstance().info("  Service Charge: " + response.serviceCharge);
    logging::Logger::getInstance().info("  Installments: " + response.installments);
    logging::Logger::getInstance().info("  Approval Number: " + response.approvalNumber);
    logging::Logger::getInstance().info("  Sales Date/Time: " + response.salesDate + " " + response.salesTime);
    logging::Logger::getInstance().info("  Transaction ID: " + response.transactionId);
    logging::Logger::getInstance().info("  Merchant Number: " + response.merchantNumber);
    logging::Logger::getInstance().info("  Terminal Number: " + response.terminalNumber);
    
    if (response.isRejected()) {
        logging::Logger::getInstance().info("  Rejection Info: " + response.rejectionInfo);
    }
    
    logging::Logger::getInstance().info("  Issuer: " + response.issuer);
    logging::Logger::getInstance().info("  Acquirer: " + response.acquirer);
    
    return true;
}

bool SmartroProtocol::parseTransactionCancelResponse(const uint8_t* data, size_t length,
                                                      TransactionCancelResponse& response) {
    // Transaction cancel response is same structure as payment approval response
    PaymentApprovalResponse tempResponse;
    if (!parsePaymentApprovalResponse(data, length, tempResponse)) {
        return false;
    }
    
    // Copy to TransactionCancelResponse
    response.transactionType = tempResponse.transactionType;
    response.transactionMedium = tempResponse.transactionMedium;
    response.cardNumber = tempResponse.cardNumber;
    response.approvalAmount = tempResponse.approvalAmount;
    response.tax = tempResponse.tax;
    response.serviceCharge = tempResponse.serviceCharge;
    response.installments = tempResponse.installments;
    response.approvalNumber = tempResponse.approvalNumber;
    response.salesDate = tempResponse.salesDate;
    response.salesTime = tempResponse.salesTime;
    response.transactionId = tempResponse.transactionId;
    response.merchantNumber = tempResponse.merchantNumber;
    response.terminalNumber = tempResponse.terminalNumber;
    response.issuer = tempResponse.issuer;
    response.rejectionInfo = tempResponse.rejectionInfo;
    response.acquirer = tempResponse.acquirer;
    response.data = tempResponse.data;
    
    return true;
}

bool SmartroProtocol::parseLastApprovalResponse(const uint8_t* data, size_t length, 
                                               LastApprovalResponse& response) {
    // ???????? ????? B ???????? (157 bytes)
    if (!data) {
        logging::Logger::getInstance().error("Last approval response data is null");
        return false;
    }
    
    // ??? ?????????
    response.data.assign(data, data + length);
    
    logging::Logger::getInstance().info("Last approval response parsed successfully: " + 
                                       std::to_string(response.data.size()) + " bytes");
    
    if (length > 0) {
        std::string hexDump;
        for (size_t i = 0; i < length && i < 64; ++i) {  // ??? 64????? ???
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X ", data[i]);
            hexDump += hex;
        }
        if (length > 64) {
            hexDump += "...";
        }
        logging::Logger::getInstance().debug("Response data: " + hexDump);
    }
    
    return true;
}

bool SmartroProtocol::parseScreenSoundSettingResponse(const uint8_t* data, size_t length, 
                                                      ScreenSoundSettingResponse& response) {
    // ???/??? ??? ????? 3 bytes
    if (!data || length < 3) {
        logging::Logger::getInstance().error("Screen/sound setting response too short: " + 
                                            std::to_string(length) + " bytes");
        return false;
    }
    
    // ????? (CHAR, 0-9)
    response.screenBrightness = static_cast<uint8_t>(data[0] - '0');
    if (response.screenBrightness > 9) {
        response.screenBrightness = 0;
    }
    
    // ????? (CHAR, 0-9)
    response.soundVolume = static_cast<uint8_t>(data[1] - '0');
    if (response.soundVolume > 9) {
        response.soundVolume = 0;
    }
    
    // ????????(CHAR, 0-9)
    response.touchSoundVolume = static_cast<uint8_t>(data[2] - '0');
    if (response.touchSoundVolume > 9) {
        response.touchSoundVolume = 0;
    }
    
    logging::Logger::getInstance().info("Screen/sound setting response: Brightness=" + 
                                       std::to_string(response.screenBrightness) + 
                                       ", Sound=" + std::to_string(response.soundVolume) + 
                                       ", Touch=" + std::to_string(response.touchSoundVolume));
    
    return true;
}

bool SmartroProtocol::parseIcCardCheckResponse(const uint8_t* data, size_t length, 
                                               IcCardCheckResponse& response) {
    // IC ?? ?? ????? 1 byte
    if (!data || length < 1) {
        logging::Logger::getInstance().error("IC card check response too short: " + 
                                            std::to_string(length) + " bytes");
        return false;
    }
    
    response.cardStatus = static_cast<char>(data[0]);
    
    const char* statusStr = "Unknown";
    if (response.cardStatus == 'O') {
        statusStr = "IC Card Inserted";
    } else if (response.cardStatus == 'X') {
        statusStr = "No IC Card";
    }
    
    logging::Logger::getInstance().info("IC card check response: " + 
                                       std::string(1, response.cardStatus) + 
                                       " (" + statusStr + ")");
    
    return true;
}

std::string SmartroProtocol::getCurrentDateTime() {
    std::time_t now = std::time(nullptr);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now);
    
    char buffer[15];
    std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d%02d%02d%02d",
                 tm_buf.tm_year + 1900,
                 tm_buf.tm_mon + 1,
                 tm_buf.tm_mday,
                 tm_buf.tm_hour,
                 tm_buf.tm_min,
                 tm_buf.tm_sec);
    
    return std::string(buffer, 14);
}

std::vector<uint8_t> SmartroProtocol::formatTerminalId(const std::string& terminalId) {
    std::vector<uint8_t> result(16, 0x00);
    
    size_t copyLength = std::min(terminalId.length(), size_t(16));
    for (size_t i = 0; i < copyLength; ++i) {
        result[i] = static_cast<uint8_t>(terminalId[i]);
    }
    
    return result;
}

void SmartroProtocol::writeUshortLE(uint16_t value, uint8_t* buffer) {
    buffer[0] = static_cast<uint8_t>(value & 0xFF);
    buffer[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

uint16_t SmartroProtocol::readUshortLE(const uint8_t* buffer) {
    return static_cast<uint16_t>(buffer[0]) | 
           (static_cast<uint16_t>(buffer[1]) << 8);
}

uint16_t SmartroProtocol::extractDataLength(const uint8_t* header) {
    // Data Length??Header??33-34?? ????(0-based index)
    if (!header) {
        logging::Logger::getInstance().error("extractDataLength: header is null");
        return 0;
    }
    // HEADER_SIZE??35???????????33, 34???????
    return readUshortLE(header + 33);
}

char SmartroProtocol::extractJobCode(const uint8_t* header) {
    // Job Code??Header??31?? ????(0-based index)
    if (!header) {
        logging::Logger::getInstance().error("extractJobCode: header is null");
        return 0;
    }
    return static_cast<char>(header[31]);
}

} // namespace smartro
