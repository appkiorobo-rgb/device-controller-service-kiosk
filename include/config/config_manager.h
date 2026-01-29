// include/config/config_manager.h
#pragma once

#include <string>
#include <filesystem>

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
    // setSessionId(sessionId) -> folder = {cameraSavePath_}/{sessionId}, next index = 0
    void setSessionId(const std::string& sessionId);
    // Returns folder path for current session (ensures dir exists). If no session set, returns cameraSavePath_.
    std::string getSessionFolder();
    // Returns next image path: {sessionFolder}/{index}.jpg and increments index (0, 1, 2...).
    std::string getNextImagePath();

    // Ensure save directory exists
    bool ensureSaveDirectoryExists() const;
    
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
};

} // namespace config
