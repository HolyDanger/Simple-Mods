#pragma once
#include <atomic>
#include <string_view>

namespace L10n {
inline std::atomic<bool> russian{false};
enum class Key {
    None, Mods, Enabled, Disabled, Controls, Mouse, Keyboard, NoSettings,
    Disable, Enable, Description, Settings, Back, CursorDescription,
    DisableFailed, MissingDll, LoadFailed, UnsupportedAbi, InvalidApi,
    EnableFailed, FrameFailed, Count
};
inline const char* Text(Key key) {
    struct Entry { const char* en; const char* ru; };
    static constexpr Entry entries[] = {
        {"", ""}, {"Mods", "Моды"}, {"Enabled", "Включён"},
        {"Disabled", "Выключен"}, {"Controls", "Управление"},
        {"Mouse", "Мышь"}, {"Keyboard", "Клавиатура"},
        {"No additional settings.", "Дополнительных настроек нет."},
        {"Disable", "Выключить"}, {"Enable", "Включить"},
        {"Description", "Описание"}, {"Settings", "Настройки"},
        {"Back", "Назад"},
        {"Adds mouse controls to the game menus and their submenus.", "Добавляет управление мышью в меню игры и его разделах."},
        {"The mod failed to disable.", "Ошибка при выключении мода"},
        {"mod.dll was not found.", "Не найден mod.dll"},
        {"Could not load the DLL. Error code: ", "Не удалось загрузить DLL. Код ошибки: "},
        {"The mod does not support Simple Mods ABI 1.", "Мод не поддерживает Simple Mods ABI 1"},
        {"Incompatible mod API version.", "Несовместимая версия API мода"},
        {"The mod could not start.", "Мод не смог запуститься"},
        {"The mod's frame callback failed.", "Ошибка в обработчике кадра"}
    };
    static_assert(sizeof(entries) / sizeof(entries[0]) == static_cast<unsigned>(Key::Count));
    const auto index = static_cast<unsigned>(key);
    if (index >= static_cast<unsigned>(Key::Count)) return "";
    return russian.load(std::memory_order_relaxed) ? entries[index].ru : entries[index].en;
}
inline bool IsRussian(std::wstring_view culture) {
    return culture.size() >= 2 && (culture[0] == L'r' || culture[0] == L'R') &&
        (culture[1] == L'u' || culture[1] == L'U') &&
        (culture.size() == 2 || culture[2] == L'-' || culture[2] == L'_');
}
}
