// src/config/config_manager.cpp
#include "config/config_manager.h"
#include "logging/logger.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

namespace config {

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

void ConfigManager::initialize(const std::string& configPath) {
    if (configPath.empty()) {
        // Use default: current working directory / config.ini (실행 시 CWD 기준)
        std::filesystem::path defaultPath = std::filesystem::current_path() / "config.ini";
        configFilePath_ = defaultPath.string();
    } else {
        configFilePath_ = configPath;
    }
    logging::Logger::getInstance().info("Config path: " + configFilePath_);

    // Try to load from file, fallback to defaults
    if (std::filesystem::exists(configFilePath_)) {
        try {
            loadFromFile(configFilePath_);
            logging::Logger::getInstance().info("Configuration loaded from: " + configFilePath_);
        } catch (const std::exception& e) {
            logging::Logger::getInstance().warn("Failed to load config file, using defaults: " + std::string(e.what()));
            loadDefaults();
        }
    } else {
        logging::Logger::getInstance().info("Config file not found, using defaults");
        loadDefaults();
        // Save defaults to file
        try {
            saveToFile(configFilePath_);
        } catch (const std::exception& e) {
            logging::Logger::getInstance().warn("Failed to save default config: " + std::string(e.what()));
        }
    }
}

void ConfigManager::loadDefaults() {
    // Default: user Documents/AiKiosk (override via config.ini camera.save_path)
    const char* userProfile = std::getenv("USERPROFILE");
#ifdef _WIN32
    if (userProfile && userProfile[0]) {
        cameraSavePath_ = std::string(userProfile) + "\\Documents\\AiKiosk";
    } else {
        cameraSavePath_ = (std::filesystem::current_path() / "AiKiosk").string();
    }
#else
    const char* home = userProfile && userProfile[0] ? userProfile : std::getenv("HOME");
    if (home && home[0]) {
        cameraSavePath_ = std::string(home) + "/Documents/AiKiosk";
    } else {
        cameraSavePath_ = (std::filesystem::current_path() / "AiKiosk").string();
    }
#endif
    currentSessionId_.clear();
    sessionNextIndex_ = 0;
    ensureSaveDirectoryExists();
}

void ConfigManager::setSessionId(const std::string& sessionId) {
    if (sessionId != currentSessionId_) {
        currentSessionId_ = sessionId;
        sessionNextIndex_ = 0;
    }
    if (!sessionId.empty()) {
        std::filesystem::path dir = std::filesystem::path(cameraSavePath_) / sessionId;
        if (!std::filesystem::exists(dir)) {
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (!ec) {
                logging::Logger::getInstance().info("Created session folder: " + dir.string());
            }
        }
    }
}

std::string ConfigManager::getSessionFolder() {
    if (currentSessionId_.empty()) {
        return cameraSavePath_;
    }
    std::filesystem::path dir = std::filesystem::path(cameraSavePath_) / currentSessionId_;
    if (!std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }
    return dir.string();
}

std::string ConfigManager::getNextImagePath() {
    std::string folder = getSessionFolder();
    unsigned int index = sessionNextIndex_++;
    return (std::filesystem::path(folder) / (std::to_string(index) + ".jpg")).string();
}

void ConfigManager::loadFromFile(const std::string& configPath) {
    currentSessionId_.clear();
    sessionNextIndex_ = 0;

    std::ifstream file(configPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + configPath);
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Simple INI parsing: key=value
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (key == "camera.save_path") {
                cameraSavePath_ = value;
            }
        }
    }
    
    file.close();
    
    // Migrate old default "photos" (relative or .../bin/.../photos) to Documents/AiKiosk
    std::string pathNorm = cameraSavePath_;
    while (!pathNorm.empty() && (pathNorm.back() == '\\' || pathNorm.back() == '/')) pathNorm.pop_back();
    bool isOldPhotos = cameraSavePath_.empty() || pathNorm == "photos"
        || (pathNorm.size() >= 7 && (pathNorm.substr(pathNorm.size() - 6) == "/photos" || pathNorm.substr(pathNorm.size() - 6) == "\\photos"));
    if (isOldPhotos) {
        const char* userProfile = std::getenv("USERPROFILE");
#ifdef _WIN32
        if (userProfile && userProfile[0]) {
            cameraSavePath_ = std::string(userProfile) + "\\Documents\\AiKiosk";
        } else {
            cameraSavePath_ = (std::filesystem::current_path() / "AiKiosk").string();
        }
#else
        const char* home = userProfile && userProfile[0] ? userProfile : std::getenv("HOME");
        if (home && home[0]) {
            cameraSavePath_ = std::string(home) + "/Documents/AiKiosk";
        } else {
            cameraSavePath_ = (std::filesystem::current_path() / "AiKiosk").string();
        }
#endif
        logging::Logger::getInstance().info("Migrated camera.save_path to: " + cameraSavePath_);
        try {
            saveToFile(configPath);
        } catch (const std::exception& e) {
            logging::Logger::getInstance().warn("Failed to save migrated config: " + std::string(e.what()));
        }
    }
    
    // Ensure directory exists
    ensureSaveDirectoryExists();
}

void ConfigManager::saveToFile(const std::string& configPath) {
    std::ofstream file(configPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create config file: " + configPath);
    }
    
    file << "# Device Controller Service Configuration\n";
    file << "# Camera settings\n";
    file << "camera.save_path=" << cameraSavePath_ << "\n";
    
    file.close();
}

void ConfigManager::setCameraSavePath(const std::string& path) {
    cameraSavePath_ = path;
    ensureSaveDirectoryExists();
    
    // Save to file if initialized
    if (!configFilePath_.empty()) {
        try {
            saveToFile(configFilePath_);
        } catch (const std::exception& e) {
            logging::Logger::getInstance().warn("Failed to save config: " + std::string(e.what()));
        }
    }
}

bool ConfigManager::ensureSaveDirectoryExists() const {
    try {
        if (!cameraSavePath_.empty()) {
            std::filesystem::path dir(cameraSavePath_);
            if (!std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);
                logging::Logger::getInstance().info("Created camera save directory: " + cameraSavePath_);
            }
            return true;
        }
    } catch (const std::exception& e) {
        logging::Logger::getInstance().error("Failed to create save directory: " + std::string(e.what()));
        return false;
    }
    return false;
}

} // namespace config
