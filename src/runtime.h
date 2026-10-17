#pragma once
#include <windows.h>
#include <filesystem>
extern HMODULE gModule;
void Log(const char*);
DWORD WINAPI StartRuntime(void*);

