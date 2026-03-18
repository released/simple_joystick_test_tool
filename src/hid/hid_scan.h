#pragma once

#include <string>
#include <vector>

struct HidScanInfo {
    std::wstring device_path;
    std::wstring manufacturer;
    std::wstring product;
    unsigned short vendor_id = 0;
    unsigned short product_id = 0;
    unsigned short usage_page = 0;
    unsigned short usage = 0;
    unsigned short input_report_length = 0;
    unsigned short output_report_length = 0;
    unsigned short feature_report_length = 0;
    unsigned int path_hash = 0;
};

struct HidScanDiagnostics {
    std::wstring line;
};

bool EnumerateHidDevices(std::vector<HidScanInfo>* devices, std::wstring* error);
bool EnumerateHidDevicesWithDiagnostics(std::vector<HidScanInfo>* devices,
                                        std::vector<std::wstring>* diagnostics,
                                        std::wstring* error);

