#include "hid_scan.h"

#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>

#include <vector>
#include <cwctype>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace {

HANDLE OpenDeviceHandle(const std::wstring& path) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        return handle;
    }
    handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        return handle;
    }
    return CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

unsigned int HashPathFNV1a(const std::wstring& path) {
    const unsigned int kOffset = 2166136261u;
    const unsigned int kPrime = 16777619u;
    unsigned int hash = kOffset;
    for (wchar_t ch : path) {
        hash ^= static_cast<unsigned int>(ch & 0xFF);
        hash *= kPrime;
        hash ^= static_cast<unsigned int>((ch >> 8) & 0xFF);
        hash *= kPrime;
    }
    return hash;
}

bool QueryCaps(HANDLE handle, HidScanInfo* info) {
    if (!info) {
        return false;
    }
    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(handle, &preparsed)) {
        return false;
    }

    HIDP_CAPS caps = {};
    NTSTATUS status = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    if (status != HIDP_STATUS_SUCCESS) {
        return false;
    }

    info->usage_page = caps.UsagePage;
    info->usage = caps.Usage;
    info->input_report_length = caps.InputReportByteLength;
    info->output_report_length = caps.OutputReportByteLength;
    info->feature_report_length = caps.FeatureReportByteLength;
    return true;
}

std::wstring ReadHidString(BOOLEAN (__stdcall *reader)(HANDLE, PVOID, ULONG), HANDLE handle) {
    wchar_t buffer[256] = {};
    if (!reader(handle, buffer, sizeof(buffer))) {
        return L"";
    }
    size_t max_chars = sizeof(buffer) / sizeof(buffer[0]);
    size_t len = 0;
    while (len < max_chars && buffer[len] != L'\0') {
        ++len;
    }
    return std::wstring(buffer, len);
}

bool ParseHexWord(const wchar_t* text, unsigned short* value) {
    if (!text || !value) {
        return false;
    }
    unsigned int out = 0;
    for (int i = 0; i < 4; ++i) {
        wchar_t ch = text[i];
        if (!iswxdigit(ch)) {
            return false;
        }
        out <<= 4;
        if (ch >= L'0' && ch <= L'9') {
            out |= static_cast<unsigned int>(ch - L'0');
        } else if (ch >= L'a' && ch <= L'f') {
            out |= static_cast<unsigned int>(ch - L'a' + 10);
        } else if (ch >= L'A' && ch <= L'F') {
            out |= static_cast<unsigned int>(ch - L'A' + 10);
        }
    }
    *value = static_cast<unsigned short>(out);
    return true;
}

bool TryParseVidPidFromHardwareIds(const std::vector<wchar_t>& buffer, unsigned short* vid, unsigned short* pid) {
    if (!vid || !pid || buffer.empty()) {
        return false;
    }
    const wchar_t* ptr = buffer.data();
    bool found = false;
    while (*ptr != L'\0') {
        const wchar_t* vid_pos = wcsstr(ptr, L"VID_");
        const wchar_t* pid_pos = wcsstr(ptr, L"PID_");
        if (vid_pos) {
            unsigned short v = 0;
            if (ParseHexWord(vid_pos + 4, &v)) {
                *vid = v;
                found = true;
            }
        }
        if (pid_pos) {
            unsigned short p = 0;
            if (ParseHexWord(pid_pos + 4, &p)) {
                *pid = p;
                found = true;
            }
        }
        size_t len = wcslen(ptr);
        ptr += len + 1;
    }
    return found;
}

bool TryParseVidPidFromString(const std::wstring& text, unsigned short* vid, unsigned short* pid) {
    if (!vid || !pid) {
        return false;
    }
    bool found = false;
    std::wstring lower = text;
    for (auto& ch : lower) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    size_t vid_pos = lower.find(L"vid_");
    if (vid_pos != std::wstring::npos && vid_pos + 8 <= lower.size()) {
        unsigned short v = 0;
        if (ParseHexWord(lower.c_str() + vid_pos + 4, &v)) {
            *vid = v;
            found = true;
        }
    }
    size_t pid_pos = lower.find(L"pid_");
    if (pid_pos != std::wstring::npos && pid_pos + 8 <= lower.size()) {
        unsigned short p = 0;
        if (ParseHexWord(lower.c_str() + pid_pos + 4, &p)) {
            *pid = p;
            found = true;
        }
    }
    return found;
}

std::wstring ReadDeviceStringProperty(HDEVINFO info_set, SP_DEVINFO_DATA* dev_data, DWORD property) {
    if (!dev_data) {
        return L"";
    }
    DWORD data_type = 0;
    DWORD required = 0;
    SetupDiGetDeviceRegistryPropertyW(info_set, dev_data, property, &data_type, nullptr, 0, &required);
    if (required == 0) {
        return L"";
    }
    std::vector<wchar_t> buffer(required / sizeof(wchar_t) + 1);
    if (!SetupDiGetDeviceRegistryPropertyW(info_set, dev_data, property, &data_type,
                                           reinterpret_cast<PBYTE>(buffer.data()),
                                           static_cast<DWORD>(buffer.size() * sizeof(wchar_t)),
                                           nullptr)) {
        return L"";
    }
    return std::wstring(buffer.data());
}

bool FillVidPidFromRegistry(HDEVINFO info_set, SP_DEVINFO_DATA* dev_data, HidScanInfo* info) {
    if (!dev_data || !info) {
        return false;
    }
    DWORD data_type = 0;
    DWORD required = 0;
    SetupDiGetDeviceRegistryPropertyW(info_set, dev_data, SPDRP_HARDWAREID, &data_type, nullptr, 0, &required);
    if (required == 0) {
        return false;
    }
    std::vector<wchar_t> buffer(required / sizeof(wchar_t) + 2);
    if (!SetupDiGetDeviceRegistryPropertyW(info_set, dev_data, SPDRP_HARDWAREID, &data_type,
                                           reinterpret_cast<PBYTE>(buffer.data()),
                                           static_cast<DWORD>(buffer.size() * sizeof(wchar_t)),
                                           nullptr)) {
        return false;
    }
    return TryParseVidPidFromHardwareIds(buffer, &info->vendor_id, &info->product_id);
}

void FillNamesFromRegistry(HDEVINFO info_set, SP_DEVINFO_DATA* dev_data, HidScanInfo* info) {
    if (!info) {
        return;
    }
    if (info->manufacturer.empty()) {
        info->manufacturer = ReadDeviceStringProperty(info_set, dev_data, SPDRP_MFG);
    }
    if (info->product.empty()) {
        info->product = ReadDeviceStringProperty(info_set, dev_data, SPDRP_FRIENDLYNAME);
        if (info->product.empty()) {
            info->product = ReadDeviceStringProperty(info_set, dev_data, SPDRP_DEVICEDESC);
        }
    }
}

}

bool EnumerateHidDevicesWithDiagnostics(std::vector<HidScanInfo>* devices,
                                        std::vector<std::wstring>* diagnostics,
                                        std::wstring* error) {
    if (!devices) {
        if (error) {
            *error = L"Output list missing.";
        }
        return false;
    }
    devices->clear();
    if (diagnostics) {
        diagnostics->clear();
    }

    GUID hid_guid = {};
    HidD_GetHidGuid(&hid_guid);

    HDEVINFO device_info_set = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr,
                                                    DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (device_info_set == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = L"SetupDiGetClassDevs failed.";
        }
        return false;
    }

    SP_DEVICE_INTERFACE_DATA interface_data = {};

    for (DWORD index = 0;; ++index) {
        interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        if (!SetupDiEnumDeviceInterfaces(device_info_set, nullptr, &hid_guid, index, &interface_data)) {
            DWORD err = GetLastError();
            if (err == ERROR_NO_MORE_ITEMS) {
                break;
            }
            continue;
        }
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetailW(device_info_set, &interface_data, nullptr, 0, &required_size, nullptr);
        if (required_size == 0) {
            continue;
        }

        std::vector<unsigned char> detail_buffer(required_size);
        auto detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detail_buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA dev_info = {};
        dev_info.cbSize = sizeof(SP_DEVINFO_DATA);
        if (!SetupDiGetDeviceInterfaceDetailW(device_info_set, &interface_data, detail, required_size, nullptr, &dev_info)) {
            continue;
        }

        HidScanInfo info;
        info.device_path = detail->DevicePath;
        info.path_hash = HashPathFNV1a(info.device_path);

        bool opened = false;
        HANDLE handle = OpenDeviceHandle(info.device_path);
        if (handle != INVALID_HANDLE_VALUE) {
            opened = true;
        }

        if (opened) {
            HIDD_ATTRIBUTES attributes = {};
            attributes.Size = sizeof(HIDD_ATTRIBUTES);
            if (HidD_GetAttributes(handle, &attributes)) {
                info.vendor_id = attributes.VendorID;
                info.product_id = attributes.ProductID;
            } else {
                FillVidPidFromRegistry(device_info_set, &dev_info, &info);
                if (info.vendor_id == 0 && info.product_id == 0) {
                    TryParseVidPidFromString(info.device_path, &info.vendor_id, &info.product_id);
                }
            }
            info.manufacturer = ReadHidString(HidD_GetManufacturerString, handle);
            info.product = ReadHidString(HidD_GetProductString, handle);
            if (info.manufacturer.empty() || info.product.empty()) {
                FillNamesFromRegistry(device_info_set, &dev_info, &info);
            }
            QueryCaps(handle, &info);
            CloseHandle(handle);
        } else {
            FillVidPidFromRegistry(device_info_set, &dev_info, &info);
            if (info.vendor_id == 0 && info.product_id == 0) {
                TryParseVidPidFromString(info.device_path, &info.vendor_id, &info.product_id);
            }
            FillNamesFromRegistry(device_info_set, &dev_info, &info);
        }

        if (diagnostics) {
            wchar_t buffer[512] = {};
            swprintf_s(buffer, L"[%lu] open=%s vid=0x%04X pid=0x%04X mfg=%s prod=%s path=%s",
                       index,
                       opened ? L"yes" : L"no",
                       info.vendor_id,
                       info.product_id,
                       info.manufacturer.c_str(),
                       info.product.c_str(),
                       info.device_path.c_str());
            diagnostics->push_back(buffer);
        }

        if (info.vendor_id != 0 || info.product_id != 0) {
            devices->push_back(info);
        }
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return true;
}

bool EnumerateHidDevices(std::vector<HidScanInfo>* devices, std::wstring* error) {
    return EnumerateHidDevicesWithDiagnostics(devices, nullptr, error);
}

