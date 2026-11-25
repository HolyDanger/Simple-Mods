#pragma once
#include "NeighborMod.h"

enum NeighborMenuKind : uint32_t {
    NeighborMenuHidden = 0,
    NeighborMenuMain = 1,
    NeighborMenuPause = 2
};

struct NeighborContextV2 {
    uint32_t size;
    uint32_t menuKind;
    uint32_t managerOpen;
    uint32_t russian;
};

typedef void (*NeighborGameTask)(void* data);

struct NeighborHostV2 {
    uint32_t size, abi;
    void* mod;
    void (*Log)(void* mod, const char* message);
    void (*DrawText)(float x, float y, float size, uint32_t rgba, const char* utf8);
    int (*GetSetting)(void* mod, const char* key, int32_t* value);
    int (*QueueGameTask)(void* mod, NeighborGameTask task, void* data);
};

struct NeighborModV2 {
    uint32_t size, abi;
    int (*OnEnable)(const NeighborHostV2* host);
    void (*OnDisable)();
    void (*OnFrame)(const NeighborFrameV1* frame);
    void (*OnSettingsChanged)();
    void (*OnContextChanged)(const NeighborContextV2* context);
};

typedef const NeighborModV2* (*NeighborModEntryV2)(uint32_t);
