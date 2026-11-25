#include "NeighborModV2.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

static int logged = 0, drawn = 0, queued = 0;
static int32_t shown = 1;
static float size = 0;
static bool russian = false;
static NeighborGameTask task = nullptr;
static void* taskData = nullptr;

static void logMessage(void*, const char*) { ++logged; }
static void text(float, float, float fontSize, uint32_t, const char* value) {
    ++drawn;
    size = fontSize;
    russian = std::strcmp(value, "Пример мода") == 0;
}
static int getSetting(void*, const char* key, int32_t* value) {
    if (std::strcmp(key, "ShowLabel") == 0) { *value = shown; return 1; }
    if (std::strcmp(key, "FontSize") == 0) { *value = 28; return 1; }
    return 0;
}
static int queue(void*, NeighborGameTask callback, void* data) {
    ++queued;
    task = callback;
    taskData = data;
    return 1;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 1;
    const auto module = LoadLibraryW(argv[1]);
    if (!module) return 2;
    const auto entry = reinterpret_cast<NeighborModEntryV2>(GetProcAddress(module, "NeighborMod_GetAPIv2"));
    if (!entry || entry(1)) return 3;
    const auto* api = entry(2);
    if (!api || api->size != sizeof(NeighborModV2) || api->abi != 2) return 4;
    NeighborHostV2 host{sizeof(host), 2, nullptr, logMessage, text, getSetting, queue};
    if (!api->OnEnable(&host) || logged != 1) return 5;
    NeighborFrameV1 frame{sizeof(frame), 1280, 720, .016f, 1};
    NeighborContextV2 context{sizeof(context), NeighborMenuMain, 1, 1};
    api->OnContextChanged(&context);
    api->OnFrame(&frame);
    if (drawn != 1 || size != 28 || !russian || queued != 1 || !task) return 6;
    task(taskData);
    if (logged != 2) return 7;
    shown = 0;
    api->OnSettingsChanged();
    api->OnFrame(&frame);
    if (drawn != 1 || queued != 1) return 8;
    api->OnDisable();
    FreeLibrary(module);
    puts("PASS: SDK 2 export, settings and context callbacks, example task");
}
