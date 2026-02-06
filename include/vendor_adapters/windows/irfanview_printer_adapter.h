// include/vendor_adapters/windows/irfanview_printer_adapter.h
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
#include <mutex>

namespace windows {

/// IrfanView-based printer adapter (Windows).
/// Prints images by launching IrfanView with /print=PrinterName.
/// Matches kio-sface-kiosk IrfanViewPrinterService behavior.
class IrfanViewPrinterAdapter : public devices::IPrinter {
public:
    explicit IrfanViewPrinterAdapter(const std::string& deviceId);
    ~IrfanViewPrinterAdapter() override = default;

    devices::DeviceInfo getDeviceInfo() const override;
    bool print(const std::string& jobId, const std::vector<uint8_t>& printData, const std::string& printerName = "") override;
    devices::DeviceState getState() const override;
    bool reset() override;
    devices::PrinterCapabilities getCapabilities() const override;
    void setPrintJobCompleteCallback(std::function<void(const devices::PrintJobCompleteEvent&)> callback) override;
    void setStateChangedCallback(std::function<void(devices::DeviceState)> callback) override;

    /// Set default printer name when payload does not specify one (e.g. "DS-RX1").
    void setDefaultPrinterName(const std::string& name) { defaultPrinterName_ = name; }
    const std::string& getDefaultPrinterName() const { return defaultPrinterName_; }

private:
    std::string deviceId_;
    std::string defaultPrinterName_;
    mutable std::string irfanViewPath_;
    mutable bool pathResolved_ = false;
    std::function<void(const devices::PrintJobCompleteEvent&)> printJobCompleteCallback_;
    std::function<void(devices::DeviceState)> stateChangedCallback_;
    mutable std::mutex mutex_;

    /// Resolve IrfanView executable path (registry + fixed paths).
    std::string resolveIrfanViewPath() const;
    /// Run IrfanView to print file; returns true if process started and exited with 0.
    bool runPrint(const std::string& filePath, const std::string& printerName) const;
};

} // namespace windows
