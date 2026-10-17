#include "NeighborMod.h"
static const NeighborHostV1* host;
static int onEnable(const NeighborHostV1* h){if(!h||h->abi!=1||h->size<sizeof(NeighborHostV1))return 0;host=h;host->Log("ExampleMod enabled");return 1;}
static void onDisable(){if(host)host->Log("ExampleMod disabled");host=nullptr;}
static void onFrame(const NeighborFrameV1* f){if(host&&f&&f->managerOpen)host->DrawText(20,20,20,0xffffffff,"ExampleMod");}
static const NeighborModV1 mod{sizeof(NeighborModV1),1,onEnable,onDisable,onFrame};
extern "C" __declspec(dllexport) const NeighborModV1* NeighborMod_GetAPI(uint32_t abi){return abi==1?&mod:nullptr;}

