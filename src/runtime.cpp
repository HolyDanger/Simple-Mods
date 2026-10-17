#define NOMINMAX
#include "runtime.h"
#include "bridge.h"
#include "NeighborMod.h"
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
struct Mod { std::wstring id;std::string name,description,version,author,error;fs::path dir;bool enabled=false,builtin=false,active=false;HMODULE dll=nullptr;const NeighborModV1* api=nullptr;};
static std::vector<Mod> mods;
static std::wstring wide(const std::string&s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);std::wstring r(n,0);if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n);return r;}
static std::string utf8(const std::wstring&s){int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string r(n,0);if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr);return r;}
static std::wstring ini(const fs::path&p,const wchar_t* section,const wchar_t* key,const wchar_t* def=L""){wchar_t b[4096]{};GetPrivateProfileStringW(section,key,def,b,4096,p.c_str());return b;}
void Log(const char* s){std::lock_guard<std::mutex>g(logLock);if(root.empty())return;std::ofstream out(root/L"SimpleMods.log",std::ios::app);SYSTEMTIME t{};GetLocalTime(&t);char b[64];sprintf_s(b,"[%02d:%02d:%02d] ",t.wHour,t.wMinute,t.wSecond);out<<b<<s<<'\n';}
static void save(){auto path=root/L"settings.ini";WritePrivateProfileStringW(L"CursorMenu",L"Enabled",cursorEnabled?L"1":L"0",path.c_str());WritePrivateProfileStringW(L"CursorMenu",L"Control",mouseMode?L"Mouse":L"Keyboard",path.c_str());for(auto&m:mods)if(!m.builtin)WritePrivateProfileStringW(L"Mods",m.id.c_str(),m.enabled?L"1":L"0",path.c_str());Bridge::SetMouse(cursorEnabled&&mouseMode);}
static void drawText(float x,float y,float size,uint32_t col,const char* s){if(ImGui::GetCurrentContext()&&s)ImGui::GetForegroundDrawList()->AddText(font,size,{x,y},col,s);}
static NeighborHostV1 host{sizeof(host),1,Log,drawText};
static int safeEnable(const NeighborModV1* api){__try{return api->OnEnable?api->OnEnable(&host):1;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
static bool safeDisable(const NeighborModV1* api){__try{if(api->OnDisable)api->OnDisable();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool safeFrame(const NeighborModV1* api,const NeighborFrameV1* f){__try{if(api->OnFrame)api->OnFrame(f);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static const NeighborModV1* safeEntry(NeighborModEntry entry){__try{return entry(1);}__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}}
static void enable(Mod& m,bool value){
 if(m.builtin){cursorEnabled=value;m.enabled=value;m.active=value;save();return;}
 if(!value){if(m.active&&m.api&&!safeDisable(m.api))m.error="Ошибка при выключении мода";m.active=false;m.enabled=false;save();return;}
 m.error.clear();if(!m.dll){auto p=m.dir/L"mod.dll";if(!fs::is_regular_file(p)){m.error="Не найден mod.dll";return;}m.dll=LoadLibraryExW(p.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!m.dll){m.error="Не удалось загрузить DLL (код "+std::to_string(GetLastError())+")";return;}auto entry=(NeighborModEntry)GetProcAddress(m.dll,"NeighborMod_GetAPI");if(!entry){m.error="Мод не поддерживает Simple Mods ABI 1";return;}m.api=safeEntry(entry);if(!m.api||m.api->size<sizeof(NeighborModV1)||m.api->abi!=1){m.api=nullptr;m.error="Несовместимая версия API мода";return;}}
 if(m.api&&safeEnable(m.api)){m.enabled=m.active=true;Log(("Enabled mod: "+m.name).c_str());}else m.error="Мод не смог запуститься";save();
}
static void loadMods(){
 auto cfg=root/L"settings.ini";cursorEnabled=ini(cfg,L"CursorMenu",L"Enabled",L"1")==L"1";mouseMode=ini(cfg,L"CursorMenu",L"Control",L"Mouse")!=L"Keyboard";
 Mod builtin;builtin.id=L"CursorMenu";builtin.name="CursorMenu";builtin.description="Добавляет управление мышью в меню игры и его разделах.";builtin.version="0.3.0";builtin.author="Simple Mods";builtin.enabled=builtin.active=cursorEnabled;builtin.builtin=true;mods.push_back(builtin);
 std::error_code ec;fs::create_directories(root/L"mods",ec);
 for(auto&d:fs::directory_iterator(root/L"mods",ec)){if(!d.is_directory())continue;auto p=d.path()/L"mod.ini";if(!fs::is_regular_file(p))continue;Mod m;m.id=d.path().filename().wstring();m.dir=d.path();m.name=utf8(ini(p,L"Mod",L"Name",m.id.c_str()));m.description=utf8(ini(p,L"Mod",L"Description"));m.version=utf8(ini(p,L"Mod",L"Version",L"1.0"));m.author=utf8(ini(p,L"Mod",L"Author",L"Неизвестен"));m.enabled=ini(cfg,L"Mods",m.id.c_str(),L"0")==L"1";mods.push_back(m);}
 Bridge::SetMouse(cursorEnabled&&mouseMode);
}
static void toggleOpen(){open=!open.load();Bridge::managerOpen=open.load();settings=false;}
static LRESULT CALLBACK wndProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 std::unique_lock<std::recursive_mutex> guard(uiLock);
 auto menu=Bridge::Snapshot();
 if(initialized&&(open||menu.visible))ImGui_ImplWin32_WndProcHandler(w,msg,wp,lp);
 if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))){
  if(wp==VK_F6&&!menu.capture){toggleOpen();return 0;}
  if(open){if(wp==VK_ESCAPE){if(settings)settings=false;else toggleOpen();}else if(wp==VK_UP)activeMod=std::max(0,activeMod-1);else if(wp==VK_DOWN)activeMod=std::min((int)mods.size()-1,activeMod+1);else if(wp=='S')settings=!settings;else if(wp==VK_SPACE||wp==VK_RETURN)enable(mods[activeMod],!mods[activeMod].enabled);else if(settings&&(wp==VK_LEFT||wp==VK_RIGHT)&&mods[activeMod].builtin){mouseMode=!mouseMode;save();}return 0;}
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
 if(window&&Bridge::TakeActivation()){PostMessageW(window,WM_KEYDOWN,VK_RETURN,1);PostMessageW(window,WM_KEYUP,VK_RETURN,0xc0000001);}
 std::unique_lock<std::recursive_mutex> guard(uiLock);
 if(!initialized){DXGI_SWAP_CHAIN_DESC desc{};s->GetDesc(&desc);if(!IsWindow(desc.OutputWindow)||FAILED(s->GetDevice(__uuidof(ID3D11Device),(void**)&device))){guard.unlock();return originalPresent(s,sync,flags);}window=desc.OutputWindow;device->GetImmediateContext(&context);makeTarget(s);IMGUI_CHECKVERSION();ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;loadFonts();ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(device,context);SetLastError(0);oldWndProc=(WNDPROC)SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)wndProc);if(!oldWndProc)Log("Warning: WndProc installation failed");initialized=true;Log("DirectX 11 overlay initialized.");for(auto&m:mods)if(!m.builtin&&m.enabled)enable(m,true);}
 auto menu=Bridge::Snapshot();bool anyMod=false;for(auto&m:mods)if(!m.builtin&&m.active&&m.api&&m.api->OnFrame)anyMod=true;
 if(!open&&!menu.visible&&!anyMod){guard.unlock();return originalPresent(s,sync,flags);}
 if(!target)makeTarget(s);
 ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ui();NeighborFrameV1 frame{sizeof(frame),ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y,ImGui::GetIO().DeltaTime,open?1:0};for(auto&m:mods)if(!m.builtin&&m.active&&m.api&&!safeFrame(m.api,&frame)){m.active=m.enabled=false;m.error="Ошибка в обработчике кадра";Log(("Mod frame failed: "+m.name).c_str());}
 ImGui::Render();if(target){ID3D11RenderTargetView*previous=nullptr;ID3D11DepthStencilView*depth=nullptr;context->OMGetRenderTargets(1,&previous,&depth);context->OMSetRenderTargets(1,&target,nullptr);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());context->OMSetRenderTargets(1,&previous,depth);if(previous)previous->Release();if(depth)depth->Release();}
 guard.unlock();return originalPresent(s,sync,flags);
}
static HRESULT WINAPI resize(IDXGISwapChain*s,UINT n,UINT w,UINT h,DXGI_FORMAT fmt,UINT flags){std::unique_lock<std::recursive_mutex>guard(uiLock);if(target){target->Release();target=nullptr;}guard.unlock();return originalResize(s,n,w,h,fmt,flags);}
DWORD WINAPI StartRuntime(void*){
 wchar_t path[MAX_PATH]{};GetModuleFileNameW(gModule,path,MAX_PATH);root=fs::path(path).parent_path()/L"SimpleMods";std::error_code ec;fs::create_directories(root,ec);Log("Simple Mods 0.3 starting; custom C++ runtime, ABI 1.");loadMods();
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


