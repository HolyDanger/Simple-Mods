#pragma once
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <climits>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Manifest {
namespace fs = std::filesystem;

inline std::wstring Read(const fs::path& file, const wchar_t* section, const wchar_t* key,
                         const wchar_t* fallback = L"") {
    wchar_t buffer[4096]{};
    GetPrivateProfileStringW(section, key, fallback, buffer, 4096, file.c_str());
    return buffer;
}

inline std::vector<std::wstring> Split(const std::wstring& value, wchar_t delimiter) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    for (;;) {
        const auto end = value.find(delimiter, start);
        auto part = value.substr(start, end - start);
        const auto first = part.find_first_not_of(L" \t");
        if (first != std::wstring::npos)
            parts.push_back(part.substr(first, part.find_last_not_of(L" \t") - first + 1));
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return parts;
}

inline std::optional<int> Integer(const std::wstring& value) {
    std::string ascii;
    for (wchar_t c : value) {
        if ((c < L'0' || c > L'9') && c != L'-') return {};
        ascii.push_back(static_cast<char>(c));
    }
    int result = 0;
    const auto end = ascii.data() + ascii.size();
    const auto parsed = std::from_chars(ascii.data(), end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end) return {};
    return result;
}

inline bool ValidKey(const std::wstring& key) {
    if (key.empty() || key.size() > 64) return false;
    for (wchar_t c : key)
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')) return false;
    return true;
}

enum class Kind { Toggle, Integer, Choice };

struct Setting {
    std::wstring key, label, labelRu;
    Kind kind;
    int min, max, value, fallback;
    std::vector<std::wstring> choices, choicesRu;
};

inline std::vector<Setting> ReadSettings(const fs::path& file, const fs::path& values,
                                         const std::wstring& section, bool& invalid) {
    std::vector<Setting> settings;
    for (const auto& key : Split(Read(file, L"Settings", L"Keys"), L',')) {
        if (!ValidKey(key) || settings.size() >= 32 ||
            std::any_of(settings.begin(), settings.end(), [&](const Setting& s) {
                return _wcsicmp(s.key.c_str(), key.c_str()) == 0;
            })) {
            invalid = true;
            continue;
        }

        const auto path = L"Setting." + key;
        const auto type = Read(file, path.c_str(), L"Type");
        Setting setting{};
        setting.key = key;
        setting.label = Read(file, path.c_str(), L"Label", key.c_str());
        setting.labelRu = Read(file, path.c_str(), L"Label.ru");
        if (type == L"Bool") {
            setting.kind = Kind::Toggle;
            setting.max = 1;
        } else if (type == L"Int") {
            const auto min = Integer(Read(file, path.c_str(), L"Min"));
            const auto max = Integer(Read(file, path.c_str(), L"Max"));
            if (!min || !max || *min >= *max) { invalid = true; continue; }
            setting.kind = Kind::Integer;
            setting.min = *min;
            setting.max = *max;
        } else if (type == L"Choice") {
            setting.kind = Kind::Choice;
            setting.choices = Split(Read(file, path.c_str(), L"Choices"), L'|');
            setting.choicesRu = Split(Read(file, path.c_str(), L"Choices.ru"), L'|');
            if (setting.choices.size() < 2 || setting.choices.size() > 32) { invalid = true; continue; }
            setting.max = static_cast<int>(setting.choices.size()) - 1;
        } else {
            invalid = true;
            continue;
        }
        setting.fallback = std::clamp(Integer(Read(file, path.c_str(), L"Default")).value_or(setting.min),
                                      setting.min, setting.max);
        setting.value = std::clamp(Integer(Read(values, section.c_str(), key.c_str())).value_or(setting.fallback),
                                   setting.min, setting.max);
        settings.push_back(std::move(setting));
    }
    return settings;
}
}
