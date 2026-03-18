#pragma once

#include <windows.h>
#include <hidsdi.h>

#include <array>
#include <string>
#include <vector>

#include "hid_scan.h"

struct HidJoystickVisualState {
    bool valid = false;

    bool x_present = false;
    LONG x_raw = 0;
    LONG x_min = 0;
    LONG x_max = 0;
    int x_norm = 500;

    bool y_present = false;
    LONG y_raw = 0;
    LONG y_min = 0;
    LONG y_max = 0;
    int y_norm = 500;

    bool z_present = false;
    LONG z_raw = 0;
    LONG z_min = 0;
    LONG z_max = 0;
    int z_norm = 500;

    bool rz_present = false;
    LONG rz_raw = 0;
    LONG rz_min = 0;
    LONG rz_max = 0;
    int rz_norm = 500;

    bool pov_present = false;
    LONG pov_raw = 0;
    LONG pov_min = 0;
    LONG pov_max = 0;
    bool pov_centered = true;
    int pov_degrees = -1;

    std::vector<unsigned char> buttons;
};

class HidDevice {
public:
    HidDevice();
    ~HidDevice();

    static bool ProbeJoystickDescriptor(const HidScanInfo& info,
                                        HidJoystickVisualState* out_state,
                                        std::vector<std::wstring>* out_mapping_lines,
                                        std::wstring* error);

    bool Open(const HidScanInfo& info, std::wstring* error);
    void Close();
    bool IsOpen() const;

    bool ReadInputReport(std::vector<unsigned char>* out_report, DWORD timeout_ms, std::wstring* error);
    bool WriteReport(const std::vector<unsigned char>& report, std::wstring* error);
    bool DecodeJoystickState(const std::vector<unsigned char>& report, HidJoystickVisualState* out_state) const;
    bool GetJoystickMappingLines(std::vector<std::wstring>* out_lines) const;
    void GetLastWriteInfo(size_t* out_length, size_t* out_actual) const;
    void CancelPendingIo();

    const HidScanInfo& info() const { return info_; }

private:
    struct AxisCapability {
        bool present = false;
        USAGE usage = 0;
        USHORT link_collection = 0;
        UCHAR report_id = 0;
        LONG logical_min = 0;
        LONG logical_max = 0;
        USHORT bit_size = 0;
        ULONG bit_offset = 0;
    };

    void ResetJoystickCaps();
    void BuildJoystickCaps();
    bool DecodeAxisValue(const AxisCapability& cap,
                         const std::vector<unsigned char>& report,
                         LONG* out_raw,
                         int* out_norm) const;

    HANDLE read_handle_;
    HANDLE write_handle_;
    size_t last_write_length_ = 0;
    size_t last_write_actual_ = 0;
    HidScanInfo info_;

    PHIDP_PREPARSED_DATA preparsed_data_ = nullptr;
    HIDP_CAPS hid_caps_ = {};
    AxisCapability axis_x_;
    AxisCapability axis_y_;
    AxisCapability axis_z_;
    AxisCapability axis_rz_;
    AxisCapability axis_hat_;
    bool has_button_page_ = false;
    std::vector<USAGE> button_usages_;
    std::vector<std::wstring> mapping_lines_;
};

