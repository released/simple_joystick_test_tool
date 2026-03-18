#include "hid_device.h"

#include <algorithm>
#include <cwchar>
#include <vector>

namespace {

HANDLE OpenReadHandle(const std::wstring& path) {
    return CreateFileW(path.c_str(), GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
}

HANDLE OpenWriteHandle(const std::wstring& path) {
    return CreateFileW(path.c_str(), GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
}

HANDLE OpenProbeHandle(const std::wstring& path) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        return handle;
    }

    handle = CreateFileW(path.c_str(), GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        return handle;
    }

    return CreateFileW(path.c_str(), 0,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool IsDeviceRemovedError(DWORD error) {
    return error == ERROR_DEVICE_NOT_CONNECTED ||
           error == ERROR_INVALID_HANDLE ||
           error == ERROR_GEN_FAILURE ||
           error == ERROR_ACCESS_DENIED;
}

bool IsJoystickOrGamepad(USHORT usage_page, USAGE usage) {
    return usage_page == 0x0001 && (usage == 0x0004 || usage == 0x0005);
}

int NormalizeToThousand(LONG raw, LONG min_value, LONG max_value) {
    if (max_value <= min_value) {
        return 500;
    }
    LONG clamped = raw;
    if (clamped < min_value) {
        clamped = min_value;
    }
    if (clamped > max_value) {
        clamped = max_value;
    }
    const double ratio = static_cast<double>(clamped - min_value) /
                         static_cast<double>(max_value - min_value);
    int normalized = static_cast<int>(ratio * 1000.0 + 0.5);
    if (normalized < 0) {
        normalized = 0;
    }
    if (normalized > 1000) {
        normalized = 1000;
    }
    return normalized;
}

LONG SignExtendIfNeeded(ULONG value, USHORT bit_size, LONG logical_min) {
    if (bit_size == 0 || bit_size >= 32 || logical_min >= 0) {
        return static_cast<LONG>(value);
    }
    const ULONG sign_bit = 1UL << (bit_size - 1);
    const ULONG mask = (1UL << bit_size) - 1UL;
    if ((value & sign_bit) == 0) {
        return static_cast<LONG>(value & mask);
    }
    const ULONG extended = value | (~mask);
    return static_cast<LONG>(extended);
}

int PovToDegrees(LONG raw, LONG logical_min, LONG logical_max) {
    if (logical_max <= logical_min) {
        return -1;
    }

    const LONG span = logical_max - logical_min + 1;
    if (span >= 4 && span <= 8) {
        const LONG index = (raw - logical_min) % 8;
        int degrees = static_cast<int>(index * 45);
        if (degrees < 0) {
            degrees += 360;
        }
        return degrees % 360;
    }

    int degrees = 0;
    if (logical_max > 360) {
        degrees = static_cast<int>(raw / 100);
    } else {
        degrees = static_cast<int>(raw);
    }
    degrees %= 360;
    if (degrees < 0) {
        degrees += 360;
    }
    return degrees;
}

const wchar_t* AxisName(USAGE usage) {
    switch (usage) {
    case 0x30:
        return L"X";
    case 0x31:
        return L"Y";
    case 0x32:
        return L"Z";
    case 0x35:
        return L"Rz";
    case 0x39:
        return L"POV";
    default:
        return L"Usage";
    }
}

std::wstring FormatBitLocation(ULONG bit_offset, USHORT bit_size) {
    ULONG start_byte = bit_offset / 8;
    ULONG start_bit = bit_offset % 8;
    ULONG end_bit_offset = bit_offset + (bit_size > 0 ? static_cast<ULONG>(bit_size - 1) : 0);
    ULONG end_byte = end_bit_offset / 8;
    ULONG end_bit = end_bit_offset % 8;

    wchar_t buffer[128] = {};
    if (bit_size <= 1) {
        swprintf_s(buffer, L"payload[%lu] bit%lu", start_byte, start_bit);
    } else if (start_byte == end_byte) {
        swprintf_s(buffer, L"payload[%lu] bit%lu..%lu", start_byte, start_bit, end_bit);
    } else {
        swprintf_s(buffer, L"payload[%lu] bit%lu -> payload[%lu] bit%lu", start_byte, start_bit, end_byte, end_bit);
    }
    return buffer;
}

std::wstring FormatMappingLine(const wchar_t* field_name, UCHAR report_id, ULONG bit_offset, USHORT bit_size) {
    std::wstring loc = FormatBitLocation(bit_offset, bit_size);
    wchar_t buffer[256] = {};
    swprintf_s(buffer, L"%-8ls : %ls (size=%u, RID=0x%02X)", field_name, loc.c_str(), static_cast<unsigned int>(bit_size), report_id);
    return buffer;
}

}

HidDevice::HidDevice()
    : read_handle_(INVALID_HANDLE_VALUE),
      write_handle_(INVALID_HANDLE_VALUE),
      last_write_length_(0),
      last_write_actual_(0) {
    ResetJoystickCaps();
}

HidDevice::~HidDevice() {
    Close();
}

bool HidDevice::ProbeJoystickDescriptor(const HidScanInfo& info,
                                        HidJoystickVisualState* out_state,
                                        std::vector<std::wstring>* out_mapping_lines,
                                        std::wstring* error) {
    if (out_state) {
        *out_state = HidJoystickVisualState{};
    }
    if (out_mapping_lines) {
        out_mapping_lines->clear();
    }

    if (info.device_path.empty()) {
        if (error) {
            *error = L"Empty device path.";
        }
        return false;
    }

    HANDLE probe_handle = OpenProbeHandle(info.device_path);
    if (probe_handle == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = L"Failed to open device for descriptor probe.";
        }
        return false;
    }

    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(probe_handle, &preparsed) || !preparsed) {
        CloseHandle(probe_handle);
        if (error) {
            *error = L"Failed to read HID preparsed data.";
        }
        return false;
    }

    HidDevice parser;
    parser.info_ = info;
    parser.preparsed_data_ = preparsed;
    parser.BuildJoystickCaps();

    HidJoystickVisualState preview_state;
    const bool has_any = parser.axis_x_.present || parser.axis_y_.present || parser.axis_z_.present ||
                         parser.axis_rz_.present || parser.axis_hat_.present || !parser.button_usages_.empty();
    preview_state.valid = has_any;
    preview_state.x_present = parser.axis_x_.present;
    preview_state.x_min = parser.axis_x_.logical_min;
    preview_state.x_max = parser.axis_x_.logical_max;
    preview_state.y_present = parser.axis_y_.present;
    preview_state.y_min = parser.axis_y_.logical_min;
    preview_state.y_max = parser.axis_y_.logical_max;
    preview_state.z_present = parser.axis_z_.present;
    preview_state.z_min = parser.axis_z_.logical_min;
    preview_state.z_max = parser.axis_z_.logical_max;
    preview_state.rz_present = parser.axis_rz_.present;
    preview_state.rz_min = parser.axis_rz_.logical_min;
    preview_state.rz_max = parser.axis_rz_.logical_max;
    preview_state.pov_present = parser.axis_hat_.present;
    preview_state.pov_min = parser.axis_hat_.logical_min;
    preview_state.pov_max = parser.axis_hat_.logical_max;
    preview_state.pov_centered = true;
    preview_state.pov_degrees = -1;
    preview_state.buttons.assign(parser.button_usages_.size(), 0);

    if (out_state) {
        *out_state = preview_state;
    }
    if (out_mapping_lines) {
        if (parser.mapping_lines_.empty()) {
            out_mapping_lines->push_back(L"(descriptor mapping unavailable)");
        } else {
            *out_mapping_lines = parser.mapping_lines_;
        }
    }

    HidD_FreePreparsedData(preparsed);
    parser.preparsed_data_ = nullptr;
    CloseHandle(probe_handle);
    return true;
}

bool HidDevice::Open(const HidScanInfo& info, std::wstring* error) {
    Close();

    HANDLE read_handle = OpenReadHandle(info.device_path);
    if (read_handle == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = L"Failed to open device (read).";
        }
        return false;
    }
    HANDLE write_handle = OpenWriteHandle(info.device_path);
    if (write_handle == INVALID_HANDLE_VALUE) {
        CloseHandle(read_handle);
        if (error) {
            *error = L"Failed to open device (write).";
        }
        return false;
    }

    read_handle_ = read_handle;
    write_handle_ = write_handle;
    info_ = info;

    if (HidD_GetPreparsedData(read_handle_, &preparsed_data_)) {
        BuildJoystickCaps();
    } else {
        ResetJoystickCaps();
    }
    return true;
}

void HidDevice::Close() {
    if (preparsed_data_) {
        HidD_FreePreparsedData(preparsed_data_);
        preparsed_data_ = nullptr;
    }

    if (read_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(read_handle_);
        read_handle_ = INVALID_HANDLE_VALUE;
    }
    if (write_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(write_handle_);
        write_handle_ = INVALID_HANDLE_VALUE;
    }
    last_write_length_ = 0;
    last_write_actual_ = 0;
    ResetJoystickCaps();
}

bool HidDevice::IsOpen() const {
    return read_handle_ != INVALID_HANDLE_VALUE && write_handle_ != INVALID_HANDLE_VALUE;
}

bool HidDevice::ReadInputReport(std::vector<unsigned char>* out_report, DWORD timeout_ms, std::wstring* error) {
    if (!IsOpen()) {
        if (error) {
            *error = L"Device not open.";
        }
        return false;
    }
    if (!out_report) {
        if (error) {
            *error = L"Output buffer missing.";
        }
        return false;
    }
    if (info_.input_report_length == 0) {
        if (error) {
            *error = L"Device has no input reports.";
        }
        return false;
    }

    out_report->assign(info_.input_report_length, 0);

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        if (error) {
            *error = L"Failed to create read event.";
        }
        return false;
    }

    DWORD bytes_read = 0;
    BOOL ok = ReadFile(read_handle_, out_report->data(), static_cast<DWORD>(out_report->size()), &bytes_read, &overlapped);
    if (ok) {
        CloseHandle(overlapped.hEvent);
        if (bytes_read < out_report->size()) {
            out_report->resize(bytes_read);
        }
        return true;
    }

    if (GetLastError() != ERROR_IO_PENDING) {
        DWORD err = GetLastError();
        CloseHandle(overlapped.hEvent);
        if (error) {
            if (IsDeviceRemovedError(err)) {
                *error = L"Device removed.";
            } else {
                *error = L"ReadFile failed.";
            }
        }
        return false;
    }

    DWORD wait_result = WaitForSingleObject(overlapped.hEvent, timeout_ms);
    if (wait_result == WAIT_TIMEOUT) {
        CancelIo(read_handle_);
        CloseHandle(overlapped.hEvent);
        if (error) {
            *error = L"Read timed out.";
        }
        return false;
    }

    if (!GetOverlappedResult(read_handle_, &overlapped, &bytes_read, FALSE)) {
        DWORD err = GetLastError();
        CloseHandle(overlapped.hEvent);
        if (error) {
            if (IsDeviceRemovedError(err)) {
                *error = L"Device removed.";
            } else {
                *error = L"Failed to retrieve read result.";
            }
        }
        return false;
    }

    CloseHandle(overlapped.hEvent);
    if (bytes_read < out_report->size()) {
        out_report->resize(bytes_read);
    }
    return true;
}

bool HidDevice::WriteReport(const std::vector<unsigned char>& report, std::wstring* error) {
    if (!IsOpen()) {
        if (error) {
            *error = L"Device not open.";
        }
        return false;
    }
    if (info_.output_report_length == 0) {
        if (error) {
            *error = L"Device has no output reports.";
        }
        return false;
    }
    if (report.empty()) {
        if (error) {
            *error = L"Report buffer empty.";
        }
        return false;
    }

    size_t required = info_.output_report_length;
    std::vector<unsigned char> buffer;
    if (required == 64 && report.size() == 65) {
        buffer.assign(report.begin() + 1, report.begin() + 65);
    } else if (required == 65 && report.size() == 64) {
        buffer.assign(65, 0x00);
        std::copy(report.begin(), report.begin() + 64, buffer.begin() + 1);
    } else {
        buffer = report;
    }

    if (required < buffer.size()) {
        if (error) {
            *error = L"Report too long for device.";
        }
        return false;
    }

    if (buffer.size() < required) {
        buffer.resize(required, 0x00);
    }

    last_write_length_ = buffer.size();
    last_write_actual_ = 0;

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        if (error) {
            *error = L"Failed to create write event.";
        }
        return false;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(write_handle_, buffer.data(), static_cast<DWORD>(buffer.size()), &written, &overlapped);
    if (!ok) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            CloseHandle(overlapped.hEvent);
            if (error) {
                if (IsDeviceRemovedError(err)) {
                    *error = L"Device removed.";
                } else {
                    *error = L"WriteFile failed.";
                }
            }
            return false;
        }
        DWORD wait_result = WaitForSingleObject(overlapped.hEvent, 500);
        if (wait_result == WAIT_TIMEOUT) {
            CancelIo(write_handle_);
            CloseHandle(overlapped.hEvent);
            if (error) {
                *error = L"Write timed out.";
            }
            return false;
        }
        if (!GetOverlappedResult(write_handle_, &overlapped, &written, FALSE)) {
            DWORD get_err = GetLastError();
            CloseHandle(overlapped.hEvent);
            if (error) {
                if (IsDeviceRemovedError(get_err)) {
                    *error = L"Device removed.";
                } else {
                    *error = L"WriteFile failed.";
                }
            }
            return false;
        }
    }
    CloseHandle(overlapped.hEvent);
    last_write_actual_ = written;
    if (last_write_actual_ != last_write_length_) {
        if (error) {
            *error = L"Write length mismatch.";
        }
        return false;
    }

    return true;
}

bool HidDevice::DecodeJoystickState(const std::vector<unsigned char>& report, HidJoystickVisualState* out_state) const {
    if (!out_state) {
        return false;
    }
    *out_state = HidJoystickVisualState{};

    if (!preparsed_data_ || report.empty()) {
        return false;
    }

    const bool has_any_capability =
        axis_x_.present || axis_y_.present || axis_z_.present || axis_rz_.present || axis_hat_.present || !button_usages_.empty();
    if (!has_any_capability) {
        return false;
    }

    HidJoystickVisualState state;
    state.valid = true;

    LONG raw = 0;
    int norm = 500;
    if (DecodeAxisValue(axis_x_, report, &raw, &norm)) {
        state.x_present = true;
        state.x_raw = raw;
        state.x_min = axis_x_.logical_min;
        state.x_max = axis_x_.logical_max;
        state.x_norm = norm;
    }
    if (DecodeAxisValue(axis_y_, report, &raw, &norm)) {
        state.y_present = true;
        state.y_raw = raw;
        state.y_min = axis_y_.logical_min;
        state.y_max = axis_y_.logical_max;
        state.y_norm = norm;
    }
    if (DecodeAxisValue(axis_z_, report, &raw, &norm)) {
        state.z_present = true;
        state.z_raw = raw;
        state.z_min = axis_z_.logical_min;
        state.z_max = axis_z_.logical_max;
        state.z_norm = norm;
    }
    if (DecodeAxisValue(axis_rz_, report, &raw, &norm)) {
        state.rz_present = true;
        state.rz_raw = raw;
        state.rz_min = axis_rz_.logical_min;
        state.rz_max = axis_rz_.logical_max;
        state.rz_norm = norm;
    }
    if (DecodeAxisValue(axis_hat_, report, &raw, &norm)) {
        state.pov_present = true;
        state.pov_raw = raw;
        state.pov_min = axis_hat_.logical_min;
        state.pov_max = axis_hat_.logical_max;

        if (raw == -1 || raw == 0xFFFF || raw < axis_hat_.logical_min || raw > axis_hat_.logical_max) {
            state.pov_centered = true;
            state.pov_degrees = -1;
        } else {
            state.pov_centered = false;
            state.pov_degrees = PovToDegrees(raw, axis_hat_.logical_min, axis_hat_.logical_max);
            if (state.pov_degrees < 0) {
                state.pov_centered = true;
            }
        }
    }

    if (has_button_page_ && !button_usages_.empty()) {
        state.buttons.assign(button_usages_.size(), 0);

        ULONG usage_length = 64;
        std::vector<USAGE> usages(usage_length);
        NTSTATUS status = HidP_GetUsages(HidP_Input, 0x09, 0, usages.data(), &usage_length,
                                         preparsed_data_,
                                         reinterpret_cast<PCHAR>(const_cast<unsigned char*>(report.data())),
                                         static_cast<ULONG>(report.size()));
        if (status == HIDP_STATUS_BUFFER_TOO_SMALL && usage_length > usages.size()) {
            usages.resize(usage_length);
            status = HidP_GetUsages(HidP_Input, 0x09, 0, usages.data(), &usage_length,
                                    preparsed_data_,
                                    reinterpret_cast<PCHAR>(const_cast<unsigned char*>(report.data())),
                                    static_cast<ULONG>(report.size()));
        }
        if (status == HIDP_STATUS_SUCCESS) {
            for (ULONG i = 0; i < usage_length; ++i) {
                const USAGE usage = usages[i];
                for (size_t b = 0; b < button_usages_.size(); ++b) {
                    if (button_usages_[b] == usage) {
                        state.buttons[b] = 1;
                        break;
                    }
                }
            }
        }
    }

    *out_state = state;
    return true;
}

bool HidDevice::GetJoystickMappingLines(std::vector<std::wstring>* out_lines) const {
    if (!out_lines) {
        return false;
    }
    *out_lines = mapping_lines_;
    return !out_lines->empty();
}

void HidDevice::GetLastWriteInfo(size_t* out_length, size_t* out_actual) const {
    if (out_length) {
        *out_length = last_write_length_;
    }
    if (out_actual) {
        *out_actual = last_write_actual_;
    }
}

void HidDevice::CancelPendingIo() {
    if (read_handle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(read_handle_, nullptr);
    }
    if (write_handle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(write_handle_, nullptr);
    }
}

void HidDevice::ResetJoystickCaps() {
    hid_caps_ = {};
    axis_x_ = AxisCapability{};
    axis_y_ = AxisCapability{};
    axis_z_ = AxisCapability{};
    axis_rz_ = AxisCapability{};
    axis_hat_ = AxisCapability{};
    has_button_page_ = false;
    button_usages_.clear();
    mapping_lines_.clear();
}

void HidDevice::BuildJoystickCaps() {
    ResetJoystickCaps();
    if (!preparsed_data_) {
        return;
    }

    NTSTATUS caps_status = HidP_GetCaps(preparsed_data_, &hid_caps_);
    if (caps_status != HIDP_STATUS_SUCCESS) {
        return;
    }

    if (!IsJoystickOrGamepad(hid_caps_.UsagePage, hid_caps_.Usage)) {
        mapping_lines_.push_back(L"(descriptor preview is limited to joystick/gamepad)");
        return;
    }

    auto assign_axis = [this](USAGE usage, const HIDP_VALUE_CAPS& cap, ULONG bit_offset) {
        AxisCapability* target = nullptr;
        switch (usage) {
        case 0x30:
            target = &axis_x_;
            break;
        case 0x31:
            target = &axis_y_;
            break;
        case 0x32:
            target = &axis_z_;
            break;
        case 0x35:
            target = &axis_rz_;
            break;
        case 0x39:
            target = &axis_hat_;
            break;
        default:
            return;
        }
        if (target->present) {
            return;
        }
        target->present = true;
        target->usage = usage;
        target->link_collection = cap.LinkCollection;
        target->report_id = cap.ReportID;
        target->logical_min = cap.LogicalMin;
        target->logical_max = cap.LogicalMax;
        target->bit_size = cap.BitSize;
        target->bit_offset = bit_offset;
    };

    USHORT value_caps_length = hid_caps_.NumberInputValueCaps;
    if (value_caps_length > 0) {
        std::vector<HIDP_VALUE_CAPS> value_caps(value_caps_length);
        if (HidP_GetValueCaps(HidP_Input, value_caps.data(), &value_caps_length, preparsed_data_) == HIDP_STATUS_SUCCESS) {
            value_caps.resize(value_caps_length);
            for (const auto& cap : value_caps) {
                if (cap.UsagePage != 0x01) {
                    continue;
                }
                if (!cap.IsRange) {
                    assign_axis(cap.NotRange.Usage, cap, cap.BitField);
                    continue;
                }
                for (USAGE usage = cap.Range.UsageMin; usage <= cap.Range.UsageMax; ++usage) {
                    ULONG usage_index = static_cast<ULONG>(usage - cap.Range.UsageMin);
                    ULONG bit_offset = cap.BitField + usage_index * cap.BitSize;
                    assign_axis(usage, cap, bit_offset);
                    if (usage == 0xFFFF) {
                        break;
                    }
                }
            }
        }
    }

    USHORT button_caps_length = hid_caps_.NumberInputButtonCaps;
    if (button_caps_length > 0) {
        std::vector<HIDP_BUTTON_CAPS> button_caps(button_caps_length);
        if (HidP_GetButtonCaps(HidP_Input, button_caps.data(), &button_caps_length, preparsed_data_) == HIDP_STATUS_SUCCESS) {
            button_caps.resize(button_caps_length);
            struct ButtonMapEntry {
                USAGE usage = 0;
                UCHAR report_id = 0;
                ULONG bit_offset = 0;
            };
            std::vector<ButtonMapEntry> button_maps;

            for (const auto& cap : button_caps) {
                if (cap.UsagePage == 0x09) {
                    has_button_page_ = true;
                    if (!cap.IsRange) {
                        const USAGE usage = cap.NotRange.Usage;
                        if (usage > 0) {
                            button_maps.push_back(ButtonMapEntry{ usage, cap.ReportID, cap.BitField });
                        }
                        continue;
                    }
                    for (USAGE usage = cap.Range.UsageMin; usage <= cap.Range.UsageMax; ++usage) {
                        if (usage > 0) {
                            ULONG bit_offset = cap.BitField + static_cast<ULONG>(usage - cap.Range.UsageMin);
                            button_maps.push_back(ButtonMapEntry{ usage, cap.ReportID, bit_offset });
                        }
                        if (usage == 0xFFFF) {
                            break;
                        }
                    }
                }
            }

            if (!button_maps.empty()) {
                std::sort(button_maps.begin(), button_maps.end(),
                          [](const ButtonMapEntry& a, const ButtonMapEntry& b) {
                              if (a.usage != b.usage) {
                                  return a.usage < b.usage;
                              }
                              if (a.report_id != b.report_id) {
                                  return a.report_id < b.report_id;
                              }
                              return a.bit_offset < b.bit_offset;
                          });

                for (const auto& entry : button_maps) {
                    if (entry.usage == 0) {
                        continue;
                    }
                    bool exists = false;
                    for (USAGE usage : button_usages_) {
                        if (usage == entry.usage) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        button_usages_.push_back(entry.usage);
                    }
                }

                for (size_t i = 0; i < button_usages_.size(); ++i) {
                    const USAGE usage = button_usages_[i];
                    for (const auto& entry : button_maps) {
                        if (entry.usage != usage) {
                            continue;
                        }
                        std::wstring label = L"Btn" + std::to_wstring(i + 1);
                        if (usage != static_cast<USAGE>(i + 1)) {
                            label += L"(U" + std::to_wstring(usage) + L")";
                        }
                        mapping_lines_.push_back(FormatMappingLine(label.c_str(), entry.report_id, entry.bit_offset, 1));
                        break;
                    }
                }
            }
        }
    }

    if (axis_x_.present) {
        mapping_lines_.push_back(FormatMappingLine(AxisName(axis_x_.usage), axis_x_.report_id, axis_x_.bit_offset, axis_x_.bit_size));
    }
    if (axis_y_.present) {
        mapping_lines_.push_back(FormatMappingLine(AxisName(axis_y_.usage), axis_y_.report_id, axis_y_.bit_offset, axis_y_.bit_size));
    }
    if (axis_z_.present) {
        mapping_lines_.push_back(FormatMappingLine(AxisName(axis_z_.usage), axis_z_.report_id, axis_z_.bit_offset, axis_z_.bit_size));
    }
    if (axis_rz_.present) {
        mapping_lines_.push_back(FormatMappingLine(AxisName(axis_rz_.usage), axis_rz_.report_id, axis_rz_.bit_offset, axis_rz_.bit_size));
    }
    if (axis_hat_.present) {
        mapping_lines_.push_back(FormatMappingLine(AxisName(axis_hat_.usage), axis_hat_.report_id, axis_hat_.bit_offset, axis_hat_.bit_size));
    }

    if (mapping_lines_.empty()) {
        mapping_lines_.push_back(L"(Descriptor mapping unavailable for this device)");
    } else {
        std::stable_sort(mapping_lines_.begin(), mapping_lines_.end(), [](const std::wstring& a, const std::wstring& b) {
            auto key = [](const std::wstring& line) {
                if (line.find(L"X") == 0) return 0;
                if (line.find(L"Y") == 0) return 1;
                if (line.find(L"Z") == 0) return 2;
                if (line.find(L"Rz") == 0) return 3;
                if (line.find(L"POV") == 0) return 4;
                if (line.find(L"Btn") == 0) return 5;
                return 6;
            };
            auto button_index = [](const std::wstring& line) {
                if (line.find(L"Btn") != 0) {
                    return 0;
                }
                int value = 0;
                size_t pos = 3;
                while (pos < line.size() && line[pos] >= L'0' && line[pos] <= L'9') {
                    value = value * 10 + static_cast<int>(line[pos] - L'0');
                    ++pos;
                }
                return value;
            };
            int ka = key(a);
            int kb = key(b);
            if (ka != kb) {
                return ka < kb;
            }
            if (ka == 5) {
                int ia = button_index(a);
                int ib = button_index(b);
                if (ia != ib) {
                    return ia < ib;
                }
            }
            return a < b;
        });
    }
}

bool HidDevice::DecodeAxisValue(const AxisCapability& cap,
                                const std::vector<unsigned char>& report,
                                LONG* out_raw,
                                int* out_norm) const {
    if (!cap.present || !preparsed_data_ || report.empty() || !out_raw || !out_norm) {
        return false;
    }

    ULONG value = 0;
    NTSTATUS status = HidP_GetUsageValue(HidP_Input, 0x01, cap.link_collection, cap.usage, &value,
                                         preparsed_data_,
                                         reinterpret_cast<PCHAR>(const_cast<unsigned char*>(report.data())),
                                         static_cast<ULONG>(report.size()));
    if (status != HIDP_STATUS_SUCCESS) {
        return false;
    }

    const LONG raw = SignExtendIfNeeded(value, cap.bit_size, cap.logical_min);
    *out_raw = raw;
    *out_norm = NormalizeToThousand(raw, cap.logical_min, cap.logical_max);
    return true;
}
