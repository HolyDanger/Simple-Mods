#define NOMINMAX
#include "runtime.h"
#include "bridge.h"
#include "localization.h"
#include "NeighborModV2.h"
#include "mod_manifest.h"
#include <utility>
#include <deque>
#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <cmath>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace fs=std::filesystem;
static fs::path root;
static std::mutex logLock;
static std::recursive_mutex uiLock;
static HWND window=nullptr;static WNDPROC oldWndProc=nullptr;
static ID3D11Device* device=nullptr;static ID3D11DeviceContext* context=nullptr;static ID3D11RenderTargetView* target=nullptr;
static bool initialized=false,modsSelected=false,settings=false,cursorEnabled=true,mouseMode=true;
static std::atomic<bool> open=false;
static int activeMod=0,hoveredNative=-1,modListTop=0;
static double frameMs=0;static ImFont* font=nullptr;static ImFont* nativeFont=nullptr;
using Present=HRESULT(WINAPI*)(IDXGISwapChain*,UINT,UINT);static Present originalPresent=nullptr;
using Resize=HRESULT(WINAPI*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);static Resize originalResize=nullptr;
struct Mod {
    std::wstring id;
    std::string name, nameRu, description, descriptionRu, version, author, detail;
    L10n::Key error = L10n::Key::None;
    fs::path dir;
    std::vector<Manifest::Setting> fields;
    std::vector<std::wstring> requires, conflicts;
    bool enabled = false, builtin = false, active = false, autoEnable = false, manifestWarning = false;
    bool gameTaskFailed = false;
    int startState = 0;
    uint32_t contextBits = UINT32_MAX;
    HMODULE dll = nullptr;
    NeighborModV1 api1{};
    NeighborModV2 api2{};
    NeighborHostV2 host2{};
};
static bool safeMode = false;
static thread_local bool drawingModFrame = false;
static int fieldIndex = 0, fieldTop = 0;
static int pendingToggle = -1, pendingSettingMod = -1, pendingSettingIndex = 0;
static int pendingAdjustment = 0;
static std::vector<Mod> mods;
static std::wstring wide(const std::string&s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);std::wstring r(n,0);if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n);return r;}
static std::string utf8(const std::wstring&s){int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string r(n,0);if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr);return r;}
static std::wstring ini(const fs::path&p,const wchar_t* section,const wchar_t* key,const wchar_t* def=L""){wchar_t b[4096]{};GetPrivateProfileStringW(section,key,def,b,4096,p.c_str());return b;}
static void initialLanguage(){
 wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};GetUserDefaultLocaleName(locale,LOCALE_NAME_MAX_LENGTH);L10n::russian=L10n::IsRussian(locale);
 wchar_t local[MAX_PATH]{};if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH))return;
 auto config=fs::path(local)/L"HelloNeighbor"/L"Saved"/L"Config"/L"WindowsNoEditor";
 for(const auto* file:{L"Engine.ini",L"GameUserSettings.ini"}){auto culture=ini(config/file,L"Internationalization",L"Culture");if(!culture.empty())L10n::russian=L10n::IsRussian(culture);}
}
void Log(const char* s){std::lock_guard<std::mutex>g(logLock);if(root.empty())return;std::ofstream out(root/L"SimpleMods.log",std::ios::app);SYSTEMTIME t{};GetLocalTime(&t);char b[64];sprintf_s(b,"[%02d:%02d:%02d] ",t.wHour,t.wMinute,t.wSecond);out<<b<<s<<'\n';}
#include "mod_runtime.inl"

static void openSettings(bool show) {
    settings = show;
    fieldIndex = fieldTop = 0;
}

static void toggleOpen() {
    open = !open.load();
    Bridge::managerOpen = open.load();
    openSettings(false);
}

static void adjustSetting(Mod& mod, Manifest::Setting& field, int step) {
    int64_t value = static_cast<int64_t>(field.value) + step;
    if (field.kind == Manifest::Kind::Toggle) value = 1 - field.value;
    else if (field.kind == Manifest::Kind::Choice) {
        if (value < field.min) value = field.max;
        if (value > field.max) value = field.min;
    }
    changeSetting(mod, field, static_cast<int>(std::clamp(value,
        static_cast<int64_t>(field.min), static_cast<int64_t>(field.max))));
}

static void managerKey(WPARAM key) {
    auto& mod = mods[activeMod];
    if (key == VK_ESCAPE) {
        if (settings) openSettings(false); else toggleOpen();
    } else if (key == 'S') {
        openSettings(!settings);
    } else if (key == VK_UP || key == VK_DOWN) {
        const int step = key == VK_UP ? -1 : 1;
        if (settings && !mod.fields.empty()) {
            fieldIndex = std::clamp(fieldIndex + step, 0, static_cast<int>(mod.fields.size()) - 1);
        } else {
            const int selected = std::clamp(activeMod + step, 0, static_cast<int>(mods.size()) - 1);
            if (selected != activeMod) {
                activeMod = selected;
                openSettings(false);
            }
        }
    } else if (settings && (key == VK_LEFT || key == VK_RIGHT)) {
        if (mod.builtin) {
            mouseMode = key == VK_LEFT;
            saveCursor();
        } else if (!mod.fields.empty()) {
            pendingSettingMod = activeMod;
            pendingSettingIndex = fieldIndex;
            pendingAdjustment = key == VK_LEFT ? -1 : 1;
        }
    } else if (key == VK_SPACE || key == VK_RETURN) {
        pendingToggle = activeMod;
    }
}

static void dispatchModCallbacks(const NativeMenu& menu, const NeighborFrameV1* frame) {
    const uint32_t kind = !menu.visible ? NeighborMenuHidden :
        (menu.frontend ? NeighborMenuMain : NeighborMenuPause);
    NeighborContextV2 context{sizeof(context), kind, open ? 1u : 0u, L10n::russian ? 1u : 0u};
    const uint32_t bits = kind | (context.managerOpen << 2) | (context.russian << 3);
    for (auto& mod : mods) {
        if (mod.builtin || !mod.active) continue;
        if (mod.gameTaskFailed) {
            disableMod(mod, L10n::Key::CallbackFailed);
            continue;
        }
        ModCallbackScope scope(mod);
        if (mod.api2.OnContextChanged && mod.contextBits != bits) {
            mod.contextBits = bits;
            if (!safeContext(mod, &context)) {
                disableMod(mod, L10n::Key::CallbackFailed);
                continue;
            }
        }
        if (!frame) continue;
        drawingModFrame = true;
        const bool ok = safeFrame(mod, frame);
        drawingModFrame = false;
        if (!ok) disableMod(mod, L10n::Key::FrameFailed);
    }
}
static LRESULT CALLBACK wndProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 std::unique_lock<std::recursive_mutex> guard(uiLock);
 auto menu=Bridge::Snapshot(false);
 if(initialized&&(open||menu.visible))ImGui_ImplWin32_WndProcHandler(w,msg,wp,lp);
 if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))){
  if(wp==VK_F6&&!menu.capture){toggleOpen();return 0;}
  if(open){managerKey(wp);return 0;}
  if(menu.visible&&menu.main){if(menu.frontend){if(wp==VK_RETURN&&menu.selected==menu.modIndex){toggleOpen();return 0;}}else{if(!modsSelected&&((wp==VK_DOWN&&menu.selected==5)||(wp==VK_UP&&menu.selected==6))){modsSelected=true;Bridge::SetModFocus(true);return 0;}if(modsSelected){if(wp==VK_UP||wp==VK_DOWN){modsSelected=false;Bridge::SetModFocus(false);Bridge::Select(wp==VK_UP?5:6,false);return 0;}if(wp==VK_RETURN){toggleOpen();return 0;}}}}
 }
 if(open&&(msg>=WM_KEYFIRST&&msg<=WM_KEYLAST||msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST||msg==WM_INPUT))return 0;
 if(cursorEnabled&&mouseMode&&menu.visible&&!menu.capture&&(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||msg==WM_RBUTTONDOWN||msg==WM_RBUTTONUP))return 0;
 if(msg==WM_KILLFOCUS){hoveredNative=-1;}
 guard.unlock();return CallWindowProcW(oldWndProc,w,msg,wp,lp);
}
static void quad(ImDrawList*d,float x,float y,float w,float h,ImU32 color,float slant=12){d->AddQuadFilled({x+slant,y},{x+w,y+4},{x+w-slant,y+h},{x,y+h-5},color);}
static bool hit(float x,float y,float w,float h){auto p=ImGui::GetIO().MousePos;return p.x>=x&&p.x<=x+w&&p.y>=y&&p.y<=y+h;}
static void txt(ImDrawList*d,float x,float y,float size,ImU32 col,const char*s){d->AddText(font,size,{x,y},col,s);}
static bool button(ImDrawList*d,float x,float y,float w,float h,const char* text,bool selected=false){bool over=hit(x,y,w,h);if(over||selected)quad(d,x,y,w,h,IM_COL32(0,0,0,245),h*.27f);txt(d,x+h*.43f,y+(h-h*.56f)/2-2,h*.56f,IM_COL32_WHITE,text);return over&&ImGui::IsMouseClicked(ImGuiMouseButton_Left);}
#include "menu_ui.inl"
static void makeTarget(IDXGISwapChain*s){ID3D11Texture2D*b=nullptr;if(SUCCEEDED(s->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&b))){device->CreateRenderTargetView(b,nullptr,&target);b->Release();}}
static void loadFonts(){
 auto&io=ImGui::GetIO();wchar_t win[MAX_PATH]{};GetWindowsDirectoryW(win,MAX_PATH);
 fs::path fonts;auto search=root.parent_path();for(int i=0;i<7&&!search.empty();i++,search=search.parent_path()){auto candidate=search/L"HelloNeighbor"/L"Content"/L"Fonts";if(fs::is_directory(candidate)){fonts=candidate;break;}}
 auto normal=fonts/L"NormalFont_0_Default.ufont",native=fonts/L"FuturaOrto.ufont";
 if(!fonts.empty()&&fs::is_regular_file(normal))font=io.Fonts->AddFontFromFileTTF(utf8(normal.wstring()).c_str(),24);
 if(!font){auto fallback=fs::path(win)/L"Fonts"/L"segoeui.ttf";font=io.Fonts->AddFontFromFileTTF(utf8(fallback.wstring()).c_str(),24);}
 if(!font)font=io.Fonts->AddFontDefault();
 if(!fonts.empty()&&fs::is_regular_file(native))nativeFont=io.Fonts->AddFontFromFileTTF(utf8(native.wstring()).c_str(),28);
 if(!nativeFont)nativeFont=font;io.FontDefault=font;
}
static HRESULT WINAPI present(IDXGISwapChain*s,UINT sync,UINT flags){
 if(callbackOwner)return originalPresent(s,sync,flags);
 if(window&&Bridge::TakeActivation()){PostMessageW(window,WM_KEYDOWN,VK_RETURN,1);PostMessageW(window,WM_KEYUP,VK_RETURN,0xc0000001);}
 std::unique_lock<std::recursive_mutex> guard(uiLock);
 if(!initialized){DXGI_SWAP_CHAIN_DESC desc{};s->GetDesc(&desc);if(!IsWindow(desc.OutputWindow)||FAILED(s->GetDevice(__uuidof(ID3D11Device),(void**)&device))){guard.unlock();return originalPresent(s,sync,flags);}window=desc.OutputWindow;device->GetImmediateContext(&context);makeTarget(s);IMGUI_CHECKVERSION();ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;loadFonts();ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(device,context);SetLastError(0);oldWndProc=(WNDPROC)SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)wndProc);if(!oldWndProc)Log("Warning: WndProc installation failed");initialized=true;Log("DirectX 11 overlay initialized.");if(!safeMode)for(auto& mod:mods)startMod(mod);}
 if(pendingToggle >= 0) {
     auto& mod = mods[std::exchange(pendingToggle, -1)];
     enable(mod, !mod.enabled);
 }
 if(pendingSettingMod >= 0) {
     auto& mod = mods[std::exchange(pendingSettingMod, -1)];
     adjustSetting(mod, mod.fields[pendingSettingIndex], pendingAdjustment);
 }
 const auto menu = Bridge::Snapshot(!open.load());
 const bool anyFrame = std::any_of(mods.begin(), mods.end(), [](const Mod& mod) {
     return mod.active && (mod.api1.OnFrame || mod.api2.OnFrame);
 });
 if(!open && !menu.visible && !anyFrame) {
     if (modsSelected) {
         modsSelected = false;
         Bridge::SetModFocus(false);
     }
     dispatchModCallbacks(menu, nullptr);
     guard.unlock();
     return originalPresent(s, sync, flags);
 }
 if(!target)makeTarget(s);
 ImGui_ImplDX11_NewFrame();
 ImGui_ImplWin32_NewFrame();
 ImGui::NewFrame();
 ui(menu);
 const auto& io = ImGui::GetIO();
 NeighborFrameV1 frame{sizeof(frame), io.DisplaySize.x, io.DisplaySize.y, io.DeltaTime, open ? 1 : 0};
 dispatchModCallbacks(menu, &frame);
 ImGui::Render();if(target){ID3D11RenderTargetView*previous=nullptr;ID3D11DepthStencilView*depth=nullptr;context->OMGetRenderTargets(1,&previous,&depth);context->OMSetRenderTargets(1,&target,nullptr);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());context->OMSetRenderTargets(1,&previous,depth);if(previous)previous->Release();if(depth)depth->Release();}
 guard.unlock();return originalPresent(s,sync,flags);
}
static HRESULT WINAPI resize(IDXGISwapChain*s,UINT n,UINT w,UINT h,DXGI_FORMAT fmt,UINT flags){std::unique_lock<std::recursive_mutex>guard(uiLock);if(target){target->Release();target=nullptr;}guard.unlock();return originalResize(s,n,w,h,fmt,flags);}
DWORD WINAPI StartRuntime(void*){
 wchar_t path[MAX_PATH]{};GetModuleFileNameW(gModule,path,MAX_PATH);root=fs::path(path).parent_path()/L"SimpleMods";std::error_code ec;fs::create_directories(root,ec);initialLanguage();Log("Simple Mods 0.5 starting (ABI 1 and 2).");loadMods();
 if(MH_Initialize()!=MH_OK){Log("MinHook initialization failed");return 1;}
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=gModule;wc.lpszClassName=L"NeighborModsProbe";RegisterClassW(&wc);HWND dummy=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,gModule,nullptr);
 DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=1;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=dummy;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;IDXGISwapChain*chain=nullptr;ID3D11Device*dev=nullptr;ID3D11DeviceContext*ctx=nullptr;
 HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&dev,nullptr,&ctx);
 if(FAILED(hr)){Log("Could not create D3D11 hook probe");DestroyWindow(dummy);return 2;}
 auto vt=*(void***)chain;void* p=vt[8];void* r=vt[13];bool ok=MH_CreateHook(p,(void*)present,(void**)&originalPresent)==MH_OK&&MH_CreateHook(r,(void*)resize,(void**)&originalResize)==MH_OK;
 chain->Release();ctx->Release();dev->Release();DestroyWindow(dummy);UnregisterClassW(wc.lpszClassName,gModule);
 if(!ok){Log("D3D11 hook creation failed");return 3;}
 if(MH_EnableHook(p)!=MH_OK||MH_EnableHook(r)!=MH_OK){Log("D3D11 hook enable failed");return 4;}Log("D3D11 callbacks installed.");
 for(int i=0;i<60;i++){if(Bridge::Init(Log))return 0;Sleep(1000);}Log("Native menu unavailable; use F6 for manager.");return 0;
}


