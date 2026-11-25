#pragma once
#include <atomic>
#include <string_view>

namespace L10n {
inline std::atomic<bool> russian{false};
enum class Key {
    None, Mods, Enabled, Disabled, Controls, Mouse, Keyboard, NoSettings,
    Disable, Enable, Description, Settings, Back, CursorDescription,
    DisableFailed, MissingDll, LoadFailed, UnsupportedAbi, InvalidApi,
    EnableFailed, FrameFailed, Dependency, RequiredBy, Conflict, Cycle, SafeMode,
    SaveFailed, CallbackFailed, InvalidManifest, Reset, Count
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
        {"Missing Simple Mods API export.", "Не найден экспорт API Simple Mods"},
        {"Incompatible mod API version.", "Несовместимая версия API мода"},
        {"The mod could not start.", "Мод не смог запуститься"},
        {"The mod's frame callback failed.", "Ошибка в обработчике кадра"},
        {"Enable required mod: ", "Включите требуемый мод: "},
        {"Still required by: ", "Этот мод нужен для: "},
        {"Conflicts with enabled mod: ", "Конфликт с включённым модом: "},
        {"Circular mod dependency.", "Циклическая зависимость модов"},
        {"Safe mode: external mods are disabled.", "Безопасный режим: сторонние моды отключены"},
        {"Could not save this setting.", "Не удалось сохранить настройку"},
        {"The mod's event callback failed.", "Ошибка в обработчике события мода"},
        {"Some mod settings could not be read.", "Часть настроек мода не удалось прочитать"},
        {"Reset", "Сброс"}
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
