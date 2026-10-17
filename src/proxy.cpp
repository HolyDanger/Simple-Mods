#include "runtime.h"
#include <xinput.h>
HMODULE gModule=nullptr;
static HMODULE systemXInput(){static HMODULE dll=[](){wchar_t p[MAX_PATH]{};GetSystemDirectoryW(p,MAX_PATH);wcscat_s(p,L"\\xinput1_3.dll");return LoadLibraryW(p);}();return dll;}
template<class F> F get(const char* n){return reinterpret_cast<F>(GetProcAddress(systemXInput(),n));}
extern "C" DWORD WINAPI ProxyGetState(DWORD i,XINPUT_STATE* p){auto f=get<decltype(&XInputGetState)>("XInputGetState");return f?f(i,p):ERROR_DEVICE_NOT_CONNECTED;}
extern "C" DWORD WINAPI ProxySetState(DWORD i,XINPUT_VIBRATION* p){auto f=get<decltype(&XInputSetState)>("XInputSetState");return f?f(i,p):ERROR_DEVICE_NOT_CONNECTED;}
extern "C" DWORD WINAPI ProxyGetCapabilities(DWORD i,DWORD flags,XINPUT_CAPABILITIES* p){auto f=get<decltype(&XInputGetCapabilities)>("XInputGetCapabilities");return f?f(i,flags,p):ERROR_DEVICE_NOT_CONNECTED;}
extern "C" void WINAPI ProxyEnable(BOOL enabled){auto f=get<decltype(&XInputEnable)>("XInputEnable");if(f)f(enabled);}
extern "C" DWORD WINAPI ProxyGetDSound(DWORD i,GUID* a,GUID* b){using F=DWORD(WINAPI*)(DWORD,GUID*,GUID*);auto f=get<F>("XInputGetDSoundAudioDeviceGuids");return f?f(i,a,b):ERROR_DEVICE_NOT_CONNECTED;}
extern "C" DWORD WINAPI ProxyBattery(DWORD i,BYTE type,XINPUT_BATTERY_INFORMATION* p){auto f=get<decltype(&XInputGetBatteryInformation)>("XInputGetBatteryInformation");return f?f(i,type,p):ERROR_DEVICE_NOT_CONNECTED;}
extern "C" DWORD WINAPI ProxyKeystroke(DWORD i,DWORD r,XINPUT_KEYSTROKE* p){auto f=get<decltype(&XInputGetKeystroke)>("XInputGetKeystroke");return f?f(i,r,p):ERROR_DEVICE_NOT_CONNECTED;}
extern "C" DWORD WINAPI ProxyStateEx(DWORD i,XINPUT_STATE* p){using F=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);auto f=get<F>((const char*)100);return f?f(i,p):ProxyGetState(i,p);}
BOOL WINAPI DllMain(HMODULE m,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH){gModule=m;DisableThreadLibraryCalls(m);wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);if(wcsstr(exe,L"HelloNeighbor-Win64-Shipping.exe")||wcsstr(exe,L"NeighborTestHost.exe")){HANDLE t=CreateThread(nullptr,0,StartRuntime,nullptr,0,nullptr);if(t)CloseHandle(t);}}return TRUE;}

