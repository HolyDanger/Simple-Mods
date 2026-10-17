# SDK

`ExampleMod.cpp` — минимальный мод для ABI 1. Соберите x64 DLL с именем `mod.dll`, положите рядом `mod.ini.example`, переименовав его в `mod.ini`. Установите оба файла в `HelloNeighbor/Binaries/Win64/SimpleMods/mods/ExampleMod/`.

Сборка из корня проекта: `cmake --build build --config Release --target ExampleMod`. Результат: `build/Release/ExampleMod.dll`; при установке переименуйте его в `mod.dll`.

Полное руководство публикуется отдельно в GitBook. Заголовок `NeighborMod.h` содержит точные структуры и сигнатуры ABI.
