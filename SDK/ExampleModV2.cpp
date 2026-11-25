#include "NeighborModV2.h"

static const NeighborHostV2* host = nullptr;
static int32_t showLabel = 1;
static int32_t fontSize = 22;
static int32_t color = 0;
static bool russian = false;
static bool queued = false;

static void gameTask(void*) {
    host->Log(host->mod, "Game-thread callback completed");
}

static void readSettings() {
    host->GetSetting(host->mod, "ShowLabel", &showLabel);
    host->GetSetting(host->mod, "FontSize", &fontSize);
    host->GetSetting(host->mod, "Color", &color);
}

static int enable(const NeighborHostV2* api) {
    if (!api || api->abi != 2 || api->size < sizeof(NeighborHostV2)) return 0;
    host = api;
    queued = false;
    readSettings();
    host->Log(host->mod, "SDK example enabled");
    return 1;
}

static void disable() { host = nullptr; }

static void contextChanged(const NeighborContextV2* context) {
    russian = context->russian != 0;
}

static void frame(const NeighborFrameV1* frame) {
    if (!queued) queued = host->QueueGameTask(host->mod, gameTask, nullptr) != 0;
    if (!showLabel || !frame->managerOpen) return;
    host->DrawText(24, 24, static_cast<float>(fontSize), color == 1 ? 0xff78dcff : 0xffffffff,
                   russian ? "Пример мода" : "Example mod");
}

static const NeighborModV2 mod{sizeof(mod), 2, enable, disable, frame, readSettings, contextChanged};

extern "C" __declspec(dllexport) const NeighborModV2* NeighborMod_GetAPIv2(uint32_t abi) {
    return abi == 2 ? &mod : nullptr;
}
