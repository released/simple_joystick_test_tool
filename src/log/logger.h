#pragma once

#include <string>
#include <mutex>

class RxLogger {
public:
    RxLogger();
    ~RxLogger();

    void Enable(bool enable);
    bool IsEnabled() const { return enabled_; }

    bool WriteLine(const std::wstring& line, std::wstring* error);
    void Close();

    std::wstring CurrentFile() const { return file_path_; }

private:
    bool OpenNewFileLocked(std::wstring* error);
    void CloseLocked();

    bool OpenNewFile(std::wstring* error);
    std::wstring BuildFileName() const;

    mutable std::mutex mutex_;
    bool enabled_;
    void* file_handle_;
    std::wstring file_path_;
};

