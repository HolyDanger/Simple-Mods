#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
struct NativeRect { float x,y,w,h; int index; int adjustable=0; float sx=0,sy=0,sw=0,sh=0,value=0; };
struct NativeMenu { bool visible=false, main=false,capture=false,frontend=false; int screen=-1,selected=0,count=0,modIndex=-1; float rowGap=0; NativeRect mods{},back{}; std::vector<NativeRect> buttons; };
namespace Bridge {
extern std::atomic<bool> compatible, mouseEnabled, managerOpen;
bool Init(void (*logger)(const char*));
NativeMenu Snapshot();
void Select(int index, bool activate);
void Adjust(int index,int direction);
void Range(int index,float value);
void Back();
bool TakeActivation();
void SetModFocus(bool value);
void SetMouse(bool value);
}

