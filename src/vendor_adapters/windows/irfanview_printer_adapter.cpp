// src/vendor_adapters/windows/irfanview_printer_adapter.cpp
#include "vendor_adapters/windows/irfanview_printer_adapter.h"
#include "logging/logger.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace windows {

IrfanViewPrinterAdapter::IrfanViewPrinterAdapter(const std::string& deviceId)
    : deviceId_(deviceId)
    , defaultPrinterName_("DS-RX1") {
}

devices::DeviceInfo IrfanViewPrinterAdapter::getDeviceInfo() const {
    devices::DeviceInfo info;
    info.deviceId = deviceId_;
    info.deviceType = devices::DeviceType::PRINTER;
    info.deviceName = "IrfanView Printer (Windows)";
    info.state = getState();
    info.lastError = "";
    info.lastUpdateTime = std::chrono::system_clock::now();
    return info;
}

std::string IrfanViewPrinterAdapter::resolveIrfanViewPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pathResolved_) {
        return irfanViewPath_;
    }
    pathResolved_ = true;

#ifdef _WIN32
    const std::vector<std::wstring> candidates = {
        L"C:\\Program Files\\IrfanView\\i_view64.exe",
        L"C:\\Program Files (x86)\\IrfanView\\i_view32.exe",
        L"C:\\IrfanView\\i_view64.exe",
        L"C:\\IrfanView\\i_view32.exe",
    };
    for (const auto& p : candidates) {
        DWORD attrs = GetFileAttributesW(p.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string result(p.begin(), p.end());
            irfanViewPath_ = result;
            logging::Logger::getInstance().info("IrfanView found: " + result);
            return result;
        }
    }
    // Registry: HKEY_LOCAL_MACHINE\SOFTWARE\...\IrfanView InstallLocation
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\IrfanView", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        wchar_t buf[1024] = {};
        DWORD size = sizeof(buf);
        if (RegQueryValueExW(key, L"InstallLocation", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS) {
            std::wstring base(buf);
            while (!base.empty() && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();
            for (const wchar_t* exe : { L"i_view64.exe", L"i_view32.exe" }) {
                std::wstring full = base + L"\\" + exe;
                if (GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    std::string result(full.begin(), full.end());
                    irfanViewPath_ = result;
                    RegCloseKey(key);
                    logging::Logger::getInstance().info("IrfanView found via registry: " + result);
                    return result;
                }
            }
        }
        RegCloseKey(key);
    }
    logging::Logger::getInstance().warn("IrfanView not found; printer will report DISCONNECTED");
#else
    (void)irfanViewPath_;
#endif
    return "";
}

devices::DeviceState IrfanViewPrinterAdapter::getState() const {
    std::string path = resolveIrfanViewPath();
    return path.empty() ? devices::DeviceState::DISCONNECTED : devices::DeviceState::STATE_READY;
}

bool IrfanViewPrinterAdapter::runPrint(const std::string& filePath, const std::string& printerName) const {
#ifdef _WIN32
    std::string exe = resolveIrfanViewPath();
    if (exe.empty()) return false;

    std::string prName = printerName.empty() ? defaultPrinterName_ : printerName;
    // IrfanView: i_view64.exe "image.jpg" /print=PrinterName
    std::wstring wexe(exe.begin(), exe.end());
    std::wstring wfile(filePath.begin(), filePath.end());
    std::wstring wprinter(prName.begin(), prName.end());

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = L"\"" + wexe + L"\" \"" + wfile + L"\" /print=" + wprinter;
    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    if (!CreateProcessW(
        wexe.c_str(),
        cmdLineBuf.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi)) {
        logging::Logger::getInstance().error("IrfanView CreateProcess failed: " + std::to_string(GetLastError()));
        return false;
    }
    CloseHandle(pi.hThread);
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 60000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    return (waitResult == WAIT_OBJECT_0 && exitCode == 0);
#else
    (void)filePath;
    (void)printerName;
    return false;
#endif
}

bool IrfanViewPrinterAdapter::print(const std::string& jobId, const std::vector<uint8_t>& printData, const std::string& printerName) {
    if (getState() != devices::DeviceState::STATE_READY) {
        if (printJobCompleteCallback_) {
            devices::PrintJobCompleteEvent ev;
            ev.jobId = jobId;
            ev.success = false;
            ev.errorMessage = "Printer not ready (IrfanView not found)";
            ev.state = getState();
            printJobCompleteCallback_(ev);
        }
        return false;
    }

#ifdef _WIN32
    wchar_t tempDir[MAX_PATH] = {};
    if (GetTempPathW(static_cast<DWORD>(std::size(tempDir)), tempDir) == 0) {
        if (printJobCompleteCallback_) {
            devices::PrintJobCompleteEvent ev;
            ev.jobId = jobId;
            ev.success = false;
            ev.errorMessage = "GetTempPath failed";
            ev.state = devices::DeviceState::STATE_ERROR;
            printJobCompleteCallback_(ev);
        }
        return false;
    }
    std::wstring tempPathW = tempDir;
    tempPathW += L"dcs_print_";
    tempPathW += std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    tempPathW += L".jpg";
    std::string tempPath(tempPathW.begin(), tempPathW.end());

    std::ofstream ofs(tempPath, std::ios::binary);
    if (!ofs || !ofs.write(reinterpret_cast<const char*>(printData.data()), static_cast<std::streamsize>(printData.size()))) {
        if (printJobCompleteCallback_) {
            devices::PrintJobCompleteEvent ev;
            ev.jobId = jobId;
            ev.success = false;
            ev.errorMessage = "Failed to write temp file";
            ev.state = devices::DeviceState::STATE_ERROR;
            printJobCompleteCallback_(ev);
        }
        return false;
    }
    ofs.close();

    std::string prName = printerName.empty() ? defaultPrinterName_ : printerName;
    bool ok = runPrint(tempPath, prName);

    DeleteFileW(tempPathW.c_str());

    if (printJobCompleteCallback_) {
        devices::PrintJobCompleteEvent ev;
        ev.jobId = jobId;
        ev.success = ok;
        ev.errorMessage = ok ? "" : "IrfanView print failed or timed out";
        ev.state = devices::DeviceState::STATE_READY;
        printJobCompleteCallback_(ev);
    }
    return ok;
#else
    (void)jobId;
    (void)printerName;
    if (printJobCompleteCallback_) {
        devices::PrintJobCompleteEvent ev;
        ev.jobId = jobId;
        ev.success = false;
        ev.errorMessage = "Windows only";
        ev.state = devices::DeviceState::STATE_ERROR;
        printJobCompleteCallback_(ev);
    }
    return false;
#endif
}

bool IrfanViewPrinterAdapter::reset() {
    return true;
}

devices::PrinterCapabilities IrfanViewPrinterAdapter::getCapabilities() const {
    devices::PrinterCapabilities cap;
    cap.supportsColor = true;
    cap.supportsDuplex = false;
    cap.supportedPaperSizes = { "4x6", "2x6" };
    cap.maxResolutionDpi = 300;
    return cap;
}

void IrfanViewPrinterAdapter::setPrintJobCompleteCallback(std::function<void(const devices::PrintJobCompleteEvent&)> callback) {
    printJobCompleteCallback_ = std::move(callback);
}

void IrfanViewPrinterAdapter::setStateChangedCallback(std::function<void(devices::DeviceState)> callback) {
    stateChangedCallback_ = std::move(callback);
}

} // namespace windows
