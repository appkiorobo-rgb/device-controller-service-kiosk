// include/config/config_manager.h
#pragma once

#include <string>
#include <filesystem>
#include <map>

namespace config {

// Configuration Manager
class ConfigManager {
public:
    static ConfigManager& getInstance();

    // Initialize configuration (load from file or use defaults)
    void initialize(const std::string& configPath = "");

    // Camera settings
    std::string getCameraSavePath() const { return cameraSavePath_; }
    void setCameraSavePath(const std::string& path);

    // Session folder (Flutter: one folder per session, timestamp id)
    void setSessionId(const std::string& sessionId);
    std::string getSessionFolder();
    std::string getNextImagePath();
    bool ensureSaveDirectoryExists() const;

    // Printer (admin / get_config / set_config)
    std::string getPrinterName() const { return printerName_; }
    void setPrinterName(const std::string& name);
    /// 용지 크기: "A4" 또는 "4x6" (인치). 바꾸면 config.ini만 수정하면 됨.
    std::string getPrinterPaperSize() const { return printerPaperSize_; }
    void setPrinterPaperSize(const std::string& size);
    int getPrinterMarginH() const { return printerMarginH_; }
    void setPrinterMarginH(int value);
    int getPrinterMarginV() const { return printerMarginV_; }
    void setPrinterMarginV(int value);

    // Payment terminal
    std::string getPaymentComPort() const { return paymentComPort_; }
    void setPaymentComPort(const std::string& port);
    bool getPaymentEnabled() const { return paymentEnabled_; }
    void setPaymentEnabled(bool value);

    // Cash device (e.g. LV77)
    std::string getCashComPort() const { return cashComPort_; }
    void setCashComPort(const std::string& port);
    bool getCashEnabled() const { return cashEnabled_; }
    void setCashEnabled(bool value);

    // Bulk get/set for IPC (key = e.g. "printer.name", "payment.com_port")
    std::map<std::string, std::string> getAll() const;
    void setFromMap(const std::map<std::string, std::string>& kv);
    void saveIfInitialized();

    /// detect_hardware 등에서 최신 config.ini 반영 (수동 편집·다른 프로세스 저장 대비)
    void reloadFromFileIfExists();

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void loadDefaults();
    void loadFromFile(const std::string& configPath);
    void saveToFile(const std::string& configPath);

    std::string cameraSavePath_;
    std::string configFilePath_;
    std::string currentSessionId_{};
    unsigned int sessionNextIndex_{0};

    std::string printerName_;
    std::string printerPaperSize_{"A4"};
    int printerMarginH_{0};
    int printerMarginV_{0};
    std::string paymentComPort_;
    bool paymentEnabled_{true};
    std::string cashComPort_;
    bool cashEnabled_{false};
};

} // namespace config
