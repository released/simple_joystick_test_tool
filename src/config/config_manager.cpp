#include "config_manager.h"

#include <windows.h>

namespace {

std::wstring GetModuleDir() {
    wchar_t path[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return L"";
    }
    std::wstring full(path, len);
    size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L"";
    }
    return full.substr(0, pos + 1);
}

std::wstring ToHexByte(unsigned char value) {
    wchar_t buffer[6] = {};
    swprintf_s(buffer, L"0x%02X", value);
    return buffer;
}

unsigned char ParseHexByte(const std::wstring& text, unsigned char def_value) {
    if (text.empty()) {
        return def_value;
    }
    unsigned int value = def_value;
    if (swscanf_s(text.c_str(), L"0x%x", &value) == 1 || swscanf_s(text.c_str(), L"%x", &value) == 1) {
        if (value <= 0xFF) {
            return static_cast<unsigned char>(value);
        }
    }
    return def_value;
}

}

ConfigManager::ConfigManager() : ini_path_(GetIniPath()) {}

std::wstring ConfigManager::GetIniPath() const {
    return GetModuleDir() + L"JoyStickTestTool.ini";
}

std::wstring ConfigManager::ReadString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& def_value) {
    wchar_t buffer[2048] = {};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), def_value.c_str(), buffer,
                             static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])), path.c_str());
    return buffer;
}

void ConfigManager::WriteString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& value) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path.c_str());
}

unsigned int ConfigManager::ReadUInt(const std::wstring& path, const std::wstring& section, const std::wstring& key, unsigned int def_value) {
    return GetPrivateProfileIntW(section.c_str(), key.c_str(), def_value, path.c_str());
}

bool ConfigManager::Load() {
    ini_path_ = GetIniPath();

    device_path = ReadString(ini_path_, L"Device", L"Path", L"");
    device_vid = static_cast<unsigned short>(ReadUInt(ini_path_, L"Device", L"VID", 0));
    device_pid = static_cast<unsigned short>(ReadUInt(ini_path_, L"Device", L"PID", 0));
    device_path_hash = ReadUInt(ini_path_, L"Device", L"PathHash", 0);

    interval_enabled = ReadUInt(ini_path_, L"Scheduler", L"IntervalEnabled", 0) != 0;
    interval_ms = ReadUInt(ini_path_, L"Scheduler", L"IntervalMs", 100);

    report_id = ParseHexByte(ReadString(ini_path_, L"Report", L"ReportID", L"0x00"), 0x00);

    handshake_enabled = ReadUInt(ini_path_, L"TX", L"HandshakeEnabled", 0) != 0;
    handshake_timeout_ms = ReadUInt(ini_path_, L"TX", L"HandshakeTimeout", 300);
    handshake_retry_count = static_cast<int>(ReadUInt(ini_path_, L"TX", L"HandshakeRetryCount", 5));
    limit_enabled = ReadUInt(ini_path_, L"TX", L"LimitEnabled", 0) != 0;
    limit_count = static_cast<int>(ReadUInt(ini_path_, L"TX", L"LimitCount", 10));
    echo_mode_enabled = ReadUInt(ini_path_, L"TX", L"EchoModeEnabled", 0) != 0;
    handshake_mode_enabled = ReadUInt(ini_path_, L"TX", L"HandshakeModeEnabled", 0) != 0;

    log_enabled = ReadUInt(ini_path_, L"RX", L"LogEnabled", 0) != 0;
    timestamp_enabled = ReadUInt(ini_path_, L"RX", L"TimestampEnabled", 0) != 0;

    return true;
}

bool ConfigManager::Save() const {
    std::wstring path = ini_path_.empty() ? GetIniPath() : ini_path_;

    WriteString(path, L"Device", L"Path", device_path);
    WriteString(path, L"Device", L"VID", std::to_wstring(device_vid));
    WriteString(path, L"Device", L"PID", std::to_wstring(device_pid));
    WriteString(path, L"Device", L"PathHash", std::to_wstring(device_path_hash));

    WriteString(path, L"Scheduler", L"IntervalEnabled", interval_enabled ? L"1" : L"0");
    WriteString(path, L"Scheduler", L"IntervalMs", std::to_wstring(interval_ms));

    WriteString(path, L"Report", L"ReportID", ToHexByte(report_id));

    WriteString(path, L"TX", L"HandshakeEnabled", handshake_enabled ? L"1" : L"0");
    WriteString(path, L"TX", L"HandshakeTimeout", std::to_wstring(handshake_timeout_ms));
    WriteString(path, L"TX", L"HandshakeRetryCount", std::to_wstring(handshake_retry_count));
    WriteString(path, L"TX", L"LimitEnabled", limit_enabled ? L"1" : L"0");
    WriteString(path, L"TX", L"LimitCount", std::to_wstring(limit_count));
    WriteString(path, L"TX", L"EchoModeEnabled", echo_mode_enabled ? L"1" : L"0");
    WriteString(path, L"TX", L"HandshakeModeEnabled", handshake_mode_enabled ? L"1" : L"0");

    WriteString(path, L"RX", L"LogEnabled", log_enabled ? L"1" : L"0");
    WriteString(path, L"RX", L"TimestampEnabled", timestamp_enabled ? L"1" : L"0");

    return true;
}

