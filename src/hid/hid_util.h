#pragma once

#include <string>
#include <vector>

std::wstring Trim(const std::wstring& value);
bool ParseHexBytes(const std::wstring& text, std::vector<unsigned char>* out_bytes, std::wstring* error);
std::wstring HexDump(const std::vector<unsigned char>& data);
std::wstring FormatWideString(const wchar_t* value, size_t max_chars);
std::wstring ToUpper(const std::wstring& value);

