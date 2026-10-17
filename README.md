# Simple Mods

[English](README.md) | [Русский](README.ru.md)

A native mod loader for **Hello Neighbor on Windows x64**. It adds a mod manager to the main menu and pause menu. The bundled **CursorMenu** provides mouse controls for menus, sliders and confirmation dialogs.

## Download and install

Download [Simple Mods 0.4](https://github.com/HolyDanger/Simple-Mods/raw/refs/heads/main/downloads/Simple-Mods-0.4.zip), extract the ZIP and run `Install.cmd` with the game closed. Select the folder containing `HelloNeighbor.exe`, then launch the game as usual. Open the Mods entry after Settings or press **F6**. In a Russian game menu the entry is «Моды».

The interface follows the game's language automatically, including changes made while the game is running. Russian uses the Russian interface; other game languages use English. Before the game menu is available, the loader uses the saved culture or Windows language. The installer follows the Windows display language.

To uninstall, close the game and run `Uninstall.cmd`. Settings and user mods are preserved.

Only the GOG executable with SHA-256 `AA057BF03B911838528B2F264009AE2484F2FAE50072A15F312F72645CCB1EA6` is supported. The installer checks compatibility; other builds need a separate profile.

## Documentation

[English documentation](https://simple-11.gitbook.io/simple-docs/en/) · [Русская документация](https://simple-11.gitbook.io/simple-docs/)

The guides cover installation, a first C++ mod, API callbacks, mod names and descriptions, configuration and troubleshooting.

## Development

Loader sources are in `src`, the API header is in `include`, and a minimal mod is in `SDK`. Install CMake, Git, Visual Studio with C++ tools and Windows SDK, then run `Build.ps1`. It downloads pinned Dear ImGui and MinHook versions and builds Release x64.

A mod consists of `mod.dll` and `mod.ini` in `HelloNeighbor/Binaries/Win64/SimpleMods/mods/<id>/`. Restart the game after adding files. New mods are disabled by default.

ABI 1 retains the names `NeighborModV1` and `NeighborMod_GetAPI` for binary compatibility. It provides lifecycle callbacks, logging and text drawing. It does not provide gameplay hooks or custom manager settings panels. Arbitrary UE4SS mods and Hello Mod Kit maps are not automatically compatible. Mod DLLs run inside the game process.

Dependency licenses are in `licenses`. Game assets, saves and analysis tools are not included.

## Bug reports

Contact [@themoment233 on Telegram](https://t.me/themoment233). Include your Simple Mods version and steps to reproduce the problem.
