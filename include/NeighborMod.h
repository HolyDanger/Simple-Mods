#pragma once
#include <stdint.h>
struct NeighborHostV1 {
    uint32_t size, abi;
    void (*Log)(const char* message);
    void (*DrawText)(float x, float y, float size, uint32_t rgba, const char* utf8);
};
struct NeighborFrameV1 { uint32_t size; float width, height, deltaSeconds; int managerOpen; };
struct NeighborModV1 {
    uint32_t size, abi;
    int (*OnEnable)(const NeighborHostV1* host);
    void (*OnDisable)();
    void (*OnFrame)(const NeighborFrameV1* frame);
};
typedef const NeighborModV1* (*NeighborModEntry)(uint32_t);

