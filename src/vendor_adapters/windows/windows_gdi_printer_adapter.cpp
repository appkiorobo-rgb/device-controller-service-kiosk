// src/vendor_adapters/windows/windows_gdi_printer_adapter.cpp
#include "vendor_adapters/windows/windows_gdi_printer_adapter.h"
#include "config/config_manager.h"
#include "logging/logger.h"
#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winspool.h>
#include <objbase.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winspool.lib")
#endif
#include <mutex>

namespace windows {

#ifdef _WIN32
namespace {
    /// 용지 크기는 config (printer.paper_size): A4 / 4x6. 방향은 landscape로 세로/가로.
    /// 그리기는 항상 비율 맞춤(fit)이라 용지만 바꾸면 됨.
    std::pair<HDC, std::vector<BYTE>> createPrinterDC(const std::wstring& printerNameW, bool landscape = false) {
        HDC hdc = nullptr;
        std::vector<BYTE> buf;
        HANDLE hPrinter = nullptr;
        if (!OpenPrinterW(const_cast<LPWSTR>(printerNameW.c_str()), &hPrinter, nullptr))
            return { nullptr, {} };
        LONG need = DocumentPropertiesW(nullptr, hPrinter, const_cast<LPWSTR>(printerNameW.c_str()), nullptr, nullptr, 0);
        if (need <= 0) {
            ClosePrinter(hPrinter);
            return { nullptr, {} };
        }
        buf.resize(static_cast<size_t>(need));
        DEVMODEW* pDM = reinterpret_cast<DEVMODEW*>(buf.data());
        if (DocumentPropertiesW(nullptr, hPrinter, const_cast<LPWSTR>(printerNameW.c_str()), pDM, nullptr, DM_OUT_BUFFER) != IDOK) {
            ClosePrinter(hPrinter);
            return { nullptr, {} };
        }
        std::string paperSize = config::ConfigManager::getInstance().getPrinterPaperSize();
        if (paperSize == "4x6") {
            pDM->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;
            pDM->dmPaperSize = 0;  // custom
            pDM->dmPaperWidth = 1016;   // 4 inch = 101.6 mm, in 0.1mm
            pDM->dmPaperLength = 1524;   // 6 inch = 152.4 mm
        } else {
            pDM->dmFields |= DM_PAPERSIZE;
            pDM->dmPaperSize = DMPAPER_A4;
        }
        pDM->dmFields |= DM_ORIENTATION;
        pDM->dmOrientation = landscape ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
        hdc = CreateDCW(nullptr, printerNameW.c_str(), nullptr, pDM);
        ClosePrinter(hPrinter);
        return { hdc, std::move(buf) };
    }

    std::string wideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string out(static_cast<size_t>(len), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &out[0], len, nullptr, nullptr) <= 0)
            return {};
        return out;
    }
    ULONG_PTR g_gdiplusToken = 0;
    bool g_gdiplusStarted = false;
    std::once_flag g_gdiplusInitFlag;

    void ensureGdiplusStartup() {
        std::call_once(g_gdiplusInitFlag, []() {
            Gdiplus::GdiplusStartupInput input;
            if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) == Gdiplus::Ok) {
                g_gdiplusStarted = true;
                logging::Logger::getInstance().info("GDI+ started");
            } else {
                logging::Logger::getInstance().warn("GDI+ startup failed");
            }
        });
    }
} // namespace

std::vector<std::string> WindowsGdiPrinterAdapter::getAvailablePrinterNames() {
    std::vector<std::string> names;
    DWORD needed = 0, count = 0;
    // PRINTER_ENUM_LOCAL(2) | PRINTER_ENUM_CONNECTED(4) — 일부 SDK에서 매크로 미정의 시 수치 사용
    const DWORD flags = PRINTER_ENUM_LOCAL | 4u;
    EnumPrintersW(flags, nullptr, 2, nullptr, 0, &needed, &count);
    if (needed == 0) return names;
    std::vector<BYTE> buf(needed);
    if (!EnumPrintersW(flags, nullptr, 2, buf.data(), needed, &needed, &count) || count == 0)
        return names;
    PRINTER_INFO_2W* infos = reinterpret_cast<PRINTER_INFO_2W*>(buf.data());
    for (DWORD i = 0; i < count && infos[i].pPrinterName; ++i)
        names.push_back(wideToUtf8(infos[i].pPrinterName));
    return names;
}
#else
std::vector<std::string> WindowsGdiPrinterAdapter::getAvailablePrinterNames() {
    return {};
}
#endif

WindowsGdiPrinterAdapter::WindowsGdiPrinterAdapter(const std::string& deviceId,
                                                   const std::string& printerName)
    : deviceId_(deviceId)
    , printerName_(printerName.empty() ? "Samsung CLS-6240 Series PS" : printerName) {
}

devices::DeviceInfo WindowsGdiPrinterAdapter::getDeviceInfo() const {
    devices::DeviceInfo info;
    info.deviceId = deviceId_;
    info.deviceType = devices::DeviceType::PRINTER;
    info.deviceName = "Windows GDI Printer (" + printerName_ + ")";
    info.state = getState();
    info.lastError = "";
    info.lastUpdateTime = std::chrono::system_clock::now();
    return info;
}

devices::DeviceState WindowsGdiPrinterAdapter::getState() const {
#ifdef _WIN32
    if (printerName_.empty()) return devices::DeviceState::DISCONNECTED;
    std::wstring wname(printerName_.begin(), printerName_.end());
    HDC hdc = CreateDCW(nullptr, wname.c_str(), nullptr, nullptr);
    if (!hdc) return devices::DeviceState::DISCONNECTED;
    DeleteDC(hdc);
    return devices::DeviceState::STATE_READY;
#else
    (void)printerName_;
    return devices::DeviceState::DISCONNECTED;
#endif
}

bool WindowsGdiPrinterAdapter::doPrint(const std::string& jobId,
                                       const std::vector<uint8_t>& printData,
                                       devices::PrintJobCompleteEvent& outEvent) {
    outEvent.jobId = jobId;
    outEvent.success = false;
    outEvent.state = devices::DeviceState::STATE_READY;
    outEvent.errorMessage = "";

#ifdef _WIN32
    if (printData.empty()) {
        outEvent.errorMessage = "Print data is empty";
        return false;
    }
    ensureGdiplusStartup();
    if (!g_gdiplusStarted) {
        outEvent.errorMessage = "GDI+ not available";
        return false;
    }
    if (printerName_.empty()) {
        outEvent.errorMessage = "No printer name";
        return false;
    }
    std::wstring wname(printerName_.begin(), printerName_.end());
    auto [hdc, devModeBuf] = createPrinterDC(wname);
    if (!hdc) {
        outEvent.errorMessage = "CreateDC failed (printer not found or A4 not supported?)";
        return false;
    }

    HGLOBAL hMem = GlobalAlloc(GHND, printData.size());
    if (!hMem) {
        DeleteDC(hdc);
        outEvent.errorMessage = "GlobalAlloc failed";
        return false;
    }
    void* pMem = GlobalLock(hMem);
    if (!pMem) {
        GlobalFree(hMem);
        DeleteDC(hdc);
        outEvent.errorMessage = "GlobalLock failed";
        return false;
    }
    memcpy(pMem, printData.data(), printData.size());
    GlobalUnlock(hMem);

    IStream* pStream = nullptr;
    HRESULT hr = CreateStreamOnHGlobal(hMem, FALSE, &pStream);
    if (FAILED(hr) || !pStream) {
        GlobalFree(hMem);
        DeleteDC(hdc);
        outEvent.errorMessage = "CreateStreamOnHGlobal failed";
        return false;
    }

    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromStream(pStream, FALSE);
    pStream->Release();
    GlobalFree(hMem);
    if (!pBitmap || pBitmap->GetLastStatus() != Gdiplus::Ok) {
        if (pBitmap) delete pBitmap;
        DeleteDC(hdc);
        outEvent.errorMessage = "Bitmap::FromStream failed";
        return false;
    }
    const int bmpW = static_cast<int>(pBitmap->GetWidth());
    const int bmpH = static_cast<int>(pBitmap->GetHeight());
    if (bmpW <= 0 || bmpH <= 0) {
        delete pBitmap;
        DeleteDC(hdc);
        outEvent.errorMessage = "Bitmap has invalid dimensions";
        return false;
    }

    DOCINFOW di = {};
    di.cbSize = sizeof(di);
    di.lpszDocName = L"Kiosk Print";
    if (StartDocW(hdc, &di) <= 0) {
        delete pBitmap;
        DeleteDC(hdc);
        outEvent.errorMessage = "StartDoc failed";
        return false;
    }
    if (StartPage(hdc) <= 0) {
        EndDoc(hdc);
        delete pBitmap;
        DeleteDC(hdc);
        outEvent.errorMessage = "StartPage failed";
        return false;
    }

    {
        Gdiplus::Graphics graphics(hdc);
        // Ensure coordinates match printer device pixels (MSDN/CodeProject: printer HDC needs explicit unit).
        graphics.SetPageUnit(Gdiplus::UnitPixel);
        int pageW = GetDeviceCaps(hdc, HORZRES);
        int pageH = GetDeviceCaps(hdc, VERTRES);
        if (pageW <= 0 || pageH <= 0) {
            logging::Logger::getInstance().warn("Printer HORZRES/VERTRES is 0, using bitmap size");
            pageW = bmpW;
            pageH = bmpH;
        }
        // Fit whole image inside page (aspect ratio preserved; may letterbox)
        double scale = (std::min)(static_cast<double>(pageW) / bmpW, static_cast<double>(pageH) / bmpH);
        int drawW = static_cast<int>(bmpW * scale + 0.5);
        int drawH = static_cast<int>(bmpH * scale + 0.5);
        int destX = (pageW - drawW) / 2;
        int destY = (pageH - drawH) / 2;
        Gdiplus::Rect destRect(destX, destY, drawW, drawH);
        Gdiplus::Status st = graphics.DrawImage(pBitmap, destRect);
        if (st != Gdiplus::Ok) {
            outEvent.errorMessage = "DrawImage failed (status=" + std::to_string(static_cast<int>(st)) + ")";
        } else {
            outEvent.success = true;
        }
    }
    EndPage(hdc);
    EndDoc(hdc);
    delete pBitmap;
    DeleteDC(hdc);
    return outEvent.success;
#else
    (void)printData;
    outEvent.errorMessage = "Windows only";
    return false;
#endif
}

bool WindowsGdiPrinterAdapter::doPrintFromFile(const std::string& jobId,
                                                const std::string& filePath,
                                                devices::PrintJobCompleteEvent& outEvent,
                                                const std::string& orientation) {
    outEvent.jobId = jobId;
    outEvent.success = false;
    outEvent.state = devices::DeviceState::STATE_READY;
    outEvent.errorMessage = "";

#ifdef _WIN32
    if (filePath.empty()) {
        outEvent.errorMessage = "filePath is empty";
        return false;
    }
    ensureGdiplusStartup();
    if (!g_gdiplusStarted) {
        outEvent.errorMessage = "GDI+ not available";
        return false;
    }
    if (printerName_.empty()) {
        outEvent.errorMessage = "No printer name";
        return false;
    }
    // UTF-8 path -> wstring for GDI+
    std::wstring wpath;
    int n = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), static_cast<int>(filePath.size()), nullptr, 0);
    if (n <= 0) {
        outEvent.errorMessage = "Path conversion failed";
        return false;
    }
    wpath.resize(static_cast<size_t>(n));
    if (MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), static_cast<int>(filePath.size()), &wpath[0], n) == 0) {
        outEvent.errorMessage = "Path conversion failed";
        return false;
    }
    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromFile(wpath.c_str(), FALSE);
    if (!pBitmap || pBitmap->GetLastStatus() != Gdiplus::Ok) {
        if (pBitmap) delete pBitmap;
        outEvent.errorMessage = "Bitmap::FromFile failed (invalid or corrupt image file)";
        return false;
    }
    const int bmpW = static_cast<int>(pBitmap->GetWidth());
    const int bmpH = static_cast<int>(pBitmap->GetHeight());
    logging::Logger::getInstance().info("printer_print from file: bitmap loaded " + std::to_string(bmpW) + "x" + std::to_string(bmpH));
    if (bmpW <= 0 || bmpH <= 0) {
        delete pBitmap;
        outEvent.errorMessage = "Bitmap has invalid dimensions";
        return false;
    }

    bool landscape = (orientation == "landscape");
    std::wstring wname(printerName_.begin(), printerName_.end());
    auto [hdc, devModeBuf] = createPrinterDC(wname, landscape);
    if (!hdc) {
        delete pBitmap;
        outEvent.errorMessage = "CreateDC failed (printer not found or A4 not supported?)";
        return false;
    }

    DOCINFOW di = {};
    di.cbSize = sizeof(di);
    di.lpszDocName = L"Kiosk Print";
    if (StartDocW(hdc, &di) <= 0) {
        delete pBitmap;
        DeleteDC(hdc);
        outEvent.errorMessage = "StartDoc failed";
        return false;
    }
    if (StartPage(hdc) <= 0) {
        EndDoc(hdc);
        delete pBitmap;
        DeleteDC(hdc);
        outEvent.errorMessage = "StartPage failed";
        return false;
    }

    {
        Gdiplus::Graphics graphics(hdc);
        // Ensure coordinates match printer device pixels (MSDN/CodeProject: printer HDC needs explicit unit).
        graphics.SetPageUnit(Gdiplus::UnitPixel);
        int pageW = GetDeviceCaps(hdc, HORZRES);
        int pageH = GetDeviceCaps(hdc, VERTRES);
        if (pageW <= 0 || pageH <= 0) {
            pageW = bmpW;
            pageH = bmpH;
        }
        // Fit whole image inside page (aspect ratio preserved; may letterbox)
        double scale = (std::min)(static_cast<double>(pageW) / bmpW, static_cast<double>(pageH) / bmpH);
        int drawW = static_cast<int>(bmpW * scale + 0.5);
        int drawH = static_cast<int>(bmpH * scale + 0.5);
        int destX = (pageW - drawW) / 2;
        int destY = (pageH - drawH) / 2;
        Gdiplus::Rect destRect(destX, destY, drawW, drawH);
        Gdiplus::Status st = graphics.DrawImage(pBitmap, destRect);
        if (st != Gdiplus::Ok) {
            outEvent.errorMessage = "DrawImage failed (status=" + std::to_string(static_cast<int>(st)) + ")";
        } else {
            outEvent.success = true;
        }
    }
    EndPage(hdc);
    EndDoc(hdc);
    delete pBitmap;
    DeleteDC(hdc);
    return outEvent.success;
#else
    (void)filePath;
    (void)orientation;
    outEvent.errorMessage = "Windows only";
    return false;
#endif
}

bool WindowsGdiPrinterAdapter::printFromFile(const std::string& jobId, const std::string& filePath, const std::string& orientation) {
    std::lock_guard<std::mutex> lock(mutex_);
    devices::PrintJobCompleteEvent ev;
    bool ok = doPrintFromFile(jobId, filePath, ev, orientation);
    if (printJobCompleteCallback_) {
        printJobCompleteCallback_(ev);
    }
    return ok;
}

bool WindowsGdiPrinterAdapter::print(const std::string& jobId,
                                     const std::vector<uint8_t>& printData) {
    std::lock_guard<std::mutex> lock(mutex_);
    devices::PrintJobCompleteEvent ev;
    bool ok = doPrint(jobId, printData, ev);
    if (printJobCompleteCallback_) {
        printJobCompleteCallback_(ev);
    }
    return ok;
}

bool WindowsGdiPrinterAdapter::reset() {
    return true;
}

devices::PrinterCapabilities WindowsGdiPrinterAdapter::getCapabilities() const {
    devices::PrinterCapabilities cap;
    cap.supportsColor = true;
    cap.supportsDuplex = false;
    cap.supportedPaperSizes = { "4x6", "2x6" };
    cap.maxResolutionDpi = 300;
    return cap;
}

void WindowsGdiPrinterAdapter::setPrintJobCompleteCallback(
    std::function<void(const devices::PrintJobCompleteEvent&)> callback) {
    printJobCompleteCallback_ = std::move(callback);
}

void WindowsGdiPrinterAdapter::setStateChangedCallback(
    std::function<void(devices::DeviceState)> callback) {
    stateChangedCallback_ = std::move(callback);
}

void WindowsGdiPrinterAdapter::setPrinterName(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    printerName_ = name;
}

} // namespace windows
