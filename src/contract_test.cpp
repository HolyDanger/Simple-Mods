#include <windows.h>
#include "NeighborMod.h"
#include <cstdio>
static int logs=0,draws=0;
static void log(const char*){logs++;}
static void draw(float,float,float,uint32_t,const char*){draws++;}
int wmain(int argc,wchar_t**argv){if(argc!=2)return 1;HMODULE dll=LoadLibraryW(argv[1]);if(!dll)return 2;auto entry=(NeighborModEntry)GetProcAddress(dll,"NeighborMod_GetAPI");if(!entry||entry(9))return 3;auto api=entry(1);if(!api||api->size!=sizeof(NeighborModV1)||api->abi!=1)return 4;NeighborHostV1 host{sizeof(host),1,log,draw};if(!api->OnEnable(&host))return 5;NeighborFrameV1 frame{sizeof(frame),1280,720,.016f,0};api->OnFrame(&frame);if(draws!=0)return 6;frame.managerOpen=1;api->OnFrame(&frame);if(draws!=1)return 7;api->OnDisable();api->OnFrame(&frame);if(draws!=1||logs!=2)return 8;if(!api->OnEnable(&host))return 9;api->OnDisable();if(logs!=4)return 10;FreeLibrary(dll);puts("PASS: ABI negotiation, enable, frame, disable, re-enable");return 0;}

