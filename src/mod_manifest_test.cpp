#include "mod_manifest.h"
#include <cstdio>

int main() {
    wchar_t temp[MAX_PATH]{}, filename[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, temp) || !GetTempFileNameW(temp, L"smd", 0, filename)) return 1;
    const auto path = std::filesystem::path(filename);
    const auto put = [&](const wchar_t* section, const wchar_t* key, const wchar_t* value) {
        return WritePrivateProfileStringW(section, key, value, filename) != FALSE;
    };
    const bool wrote = put(L"Settings", L"Keys", L"Enabled, Size, Color, Broken, size") &&
        put(L"Setting.Enabled", L"Type", L"Bool") &&
        put(L"Setting.Size", L"Type", L"Int") &&
        put(L"Setting.Size", L"Min", L"-10") &&
        put(L"Setting.Size", L"Max", L"24") &&
        put(L"Setting.Size", L"Default", L"8") &&
        put(L"Example", L"Size", L"999") &&
        put(L"Setting.Color", L"Type", L"Choice") &&
        put(L"Setting.Color", L"Choices", L"White|Red") &&
        put(L"Setting.Color", L"Default", L"1");
    bool invalid = false;
    auto settings = Manifest::ReadSettings(path, path, L"Example", invalid);
    DeleteFileW(filename);
    if (!wrote || !invalid || settings.size() != 3) return 2;
    if (settings[0].value != 0 || settings[1].value != 24 ||
        settings[1].fallback != 8 || settings[2].value != 1) return 3;
    if (Manifest::Integer(L"2147483648") || Manifest::Integer(L"2x")) return 4;
    puts("PASS: manifest validation, choice defaults and saved-value clamping");
}
