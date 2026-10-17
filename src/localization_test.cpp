#include "localization.h"
#include <cstdio>
#include <cstring>
int main() {
    if (!L10n::IsRussian(L"ru") || !L10n::IsRussian(L"ru-RU") ||
        !L10n::IsRussian(L"RU_ru") || L10n::IsRussian(L"en-US") ||
        L10n::IsRussian(L"russian") || L10n::IsRussian(L"")) return 1;
    for (bool russian : {false, true}) {
        L10n::russian = russian;
        for (unsigned i = 1; i < static_cast<unsigned>(L10n::Key::Count); ++i)
            if (!*L10n::Text(static_cast<L10n::Key>(i))) return 2;
        if (std::strcmp(L10n::Text(L10n::Key::Mods), russian ? "Моды" : "Mods")) return 3;
        if (std::strcmp(L10n::Text(L10n::Key::Settings), russian ? "Настройки" : "Settings")) return 4;
    }
    L10n::russian = false;
    if (std::strcmp(L10n::Text(L10n::Key::MissingDll), "mod.dll was not found.")) return 5;
    if (*L10n::Text(L10n::Key::Count)) return 6;
    std::puts("PASS: culture detection, complete RU/EN strings, live switching and error translation");
}
