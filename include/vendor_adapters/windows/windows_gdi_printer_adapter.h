// include/vendor_adapters/windows/windows_gdi_printer_adapter.h
#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "devices/iprinter.h"
#include <string>
#include <vector>
#include <mutex>

namespace windows {

/// Windows GDI+ printer adapter. Prints image to a named printer without external exe.
class WindowsGdiPrinterAdapter : public devices::IPrinter {
public:
    /// Windows에 등록된 프린터 이름 목록 반환 (로컬 + 네트워크). Windows 전용.
    static std::vector<std::string> getAvailablePrinterNames();

    explicit WindowsGdiPrinterAdapter(const std::string& deviceId,
                                      const std::string& printerName);
    ~WindowsGdiPrinterAdapter() override = default;

    devices::DeviceInfo getDeviceInfo() const override;
    bool print(const std::string& jobId, const std::vector<uint8_t>& printData) override;
    bool printFromFile(const std::string& jobId, const std::string& filePath, const std::string& orientation = "portrait") override;
    devices::DeviceState getState() const override;
    bool reset() override;
    devices::PrinterCapabilities getCapabilities() const override;
    void setPrintJobCompleteCallback(std::function<void(const devices::PrintJobCompleteEvent&)> callback) override;
    void setStateChangedCallback(std::function<void(devices::DeviceState)> callback) override;

    void setPrinterName(const std::string& name);
    const std::string& getPrinterName() const { return printerName_; }

private:
    std::string deviceId_;
    std::string printerName_;
    std::function<void(const devices::PrintJobCompleteEvent&)> printJobCompleteCallback_;
    std::function<void(devices::DeviceState)> stateChangedCallback_;
    mutable std::mutex mutex_;

    bool doPrint(const std::string& jobId, const std::vector<uint8_t>& printData,
                 devices::PrintJobCompleteEvent& outEvent);
    bool doPrintFromFile(const std::string& jobId, const std::string& filePath,
                         devices::PrintJobCompleteEvent& outEvent,
                         const std::string& orientation = "portrait");
};

} // namespace windows
