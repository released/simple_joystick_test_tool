#include "hid_util.h"

#include <algorithm>
#include <cwctype>
#include <sstream>

std::wstring Trim(const std::wstring& value) {
    size_t start = 0;
    while (start < value.size() && iswspace(value[start])) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && iswspace(value[end - 1])) {
        --end;
    }
    return value.substr(start, end - start);
}

std::wstring ToUpper(const std::wstring& value) {
    std::wstring out = value;
    std::transform(out.begin(), out.end(), out.begin(), towupper);
    return out;
}

static bool IsHexChar(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F');
}

static int HexValue(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') {
        return ch - L'0';
    }
    if (ch >= L'a' && ch <= L'f') {
        return 10 + (ch - L'a');
    }
    if (ch >= L'A' && ch <= L'F') {
        return 10 + (ch - L'A');
    }
    return -1;
}

bool ParseHexBytes(const std::wstring& text, std::vector<unsigned char>* out_bytes, std::wstring* error) {
    if (!out_bytes) {
        if (error) {
            *error = L"Internal error: output buffer missing.";
        }
        return false;
    }
    out_bytes->clear();

    std::wstring trimmed = Trim(text);
    if (trimmed.empty()) {
        if (error) {
            *error = L"Please enter hex bytes (e.g. 00 FF 1A).";
        }
        return false;
    }

    std::wstring clean;
    clean.reserve(trimmed.size());
    for (wchar_t ch : trimmed) {
        if (iswspace(ch) || ch == L',' || ch == L'-' || ch == L'_') {
            continue;
        }
        if (!IsHexChar(ch)) {
            if (error) {
                std::wstringstream ss;
                ss << L"Invalid hex character: '" << ch << L"'";
                *error = ss.str();
            }
            return false;
        }
        clean.push_back(ch);
    }

    if (clean.size() % 2 != 0) {
        if (error) {
            *error = L"Hex string must contain an even number of digits.";
        }
        return false;
    }

    for (size_t i = 0; i < clean.size(); i += 2) {
        int hi = HexValue(clean[i]);
        int lo = HexValue(clean[i + 1]);
        if (hi < 0 || lo < 0) {
            if (error) {
                *error = L"Invalid hex digit encountered.";
            }
            return false;
        }
        out_bytes->push_back(static_cast<unsigned char>((hi << 4) | lo));
    }

    return true;
}

std::wstring HexDump(const std::vector<unsigned char>& data) {
    std::wstringstream ss;
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) {
            ss << L' ';
        }
        ss << std::uppercase << std::hex;
        ss.width(2);
        ss.fill(L'0');
        ss << static_cast<int>(data[i]);
    }
    return ss.str();
}

std::wstring FormatWideString(const wchar_t* value, size_t max_chars) {
    if (!value) {
        return L"";
    }
    size_t len = 0;
    while (len < max_chars && value[len] != L'\0') {
        ++len;
    }
    return std::wstring(value, len);
}

