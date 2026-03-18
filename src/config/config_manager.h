#pragma once

#include <array>
#include <string>

class ConfigManager {
public:
    ConfigManager();

    bool Load();
    bool Save() const;

    const std::wstring& path() const { return ini_path_; }

    std::wstring device_path;
    unsigned short device_vid = 0;
    unsigned short device_pid = 0;
    unsigned int device_path_hash = 0;

    bool interval_enabled = false;
    unsigned int interval_ms = 100;

    unsigned char report_id = 0x00;

    bool handshake_enabled = false;
    unsigned int handshake_timeout_ms = 300;
    int handshake_retry_count = 5;
    bool limit_enabled = false;
    int limit_count = 10;
    bool echo_mode_enabled = false;
    bool handshake_mode_enabled = false;

    bool log_enabled = false;
    bool timestamp_enabled = false;

private:
    std::wstring ini_path_;

    std::wstring GetIniPath() const;
    static std::wstring ReadString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& def_value);
    static void WriteString(const std::wstring& path, const std::wstring& section, const std::wstring& key, const std::wstring& value);
    static unsigned int ReadUInt(const std::wstring& path, const std::wstring& section, const std::wstring& key, unsigned int def_value);
};

