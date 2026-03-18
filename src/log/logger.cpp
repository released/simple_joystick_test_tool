#include "logger.h"

#include <windows.h>
#include <mutex>

namespace {

std::wstring GetModuleDirectory() {
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

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return std::string();
    }
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], size, nullptr, nullptr);
    return out;
}

}

RxLogger::RxLogger() : enabled_(false), file_handle_(nullptr) {}

RxLogger::~RxLogger() {
    Close();
}

void RxLogger::Enable(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enable == enabled_) {
        return;
    }
    enabled_ = enable;
    if (!enabled_) {
        CloseLocked();
    } else {
        std::wstring error;
        OpenNewFileLocked(&error);
    }
}

void RxLogger::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    CloseLocked();
}

void RxLogger::CloseLocked() {
    if (file_handle_) {
        CloseHandle(static_cast<HANDLE>(file_handle_));
        file_handle_ = nullptr;
    }
}

std::wstring RxLogger::BuildFileName() const {
    SYSTEMTIME now = {};
    GetLocalTime(&now);

    wchar_t name[64] = {};
    swprintf_s(name, L"%04u%02u%02u_%02u%02u%02u_hid_rx.log",
               now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

    return GetModuleDirectory() + name;
}

bool RxLogger::OpenNewFile(std::wstring* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    return OpenNewFileLocked(error);
}

bool RxLogger::OpenNewFileLocked(std::wstring* error) {
    CloseLocked();

    file_path_ = BuildFileName();
    HANDLE handle = CreateFileW(file_path_.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        file_handle_ = nullptr;
        if (error) {
            *error = L"Failed to create log file.";
        }
        return false;
    }
    file_handle_ = handle;
    return true;
}

bool RxLogger::WriteLine(const std::wstring& line, std::wstring* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) {
        return false;
    }
    if (!file_handle_) {
        if (!OpenNewFileLocked(error)) {
            return false;
        }
    }

    std::string utf8_line = WideToUtf8(line + L"\r\n");
    DWORD written = 0;
    BOOL ok = WriteFile(static_cast<HANDLE>(file_handle_), utf8_line.data(),
                        static_cast<DWORD>(utf8_line.size()), &written, nullptr);
    if (!ok) {
        if (error) {
            *error = L"Failed to write log file.";
        }
        return false;
    }
    return true;
}

