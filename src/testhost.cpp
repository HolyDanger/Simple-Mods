#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <filesystem>
ID3D11RenderTargetView* rt;IDXGISwapChain* sc;ID3D11Device* dev;ID3D11DeviceContext* ctx;
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"NeighborTestHost";RegisterClassW(&wc);HWND h=CreateWindowW(wc.lpszClassName,L"NeighborMods — D3D11 test host",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,800,nullptr,nullptr,instance,nullptr);DXGI_SWAP_CHAIN_DESC d{};d.BufferCount=1;d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.OutputWindow=h;d.SampleDesc.Count=1;d.Windowed=TRUE;d.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&sc,&dev,nullptr,&ctx)))return 1;ID3D11Texture2D* b;sc->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&b);dev->CreateRenderTargetView(b,nullptr,&rt);b->Release();wchar_t p[MAX_PATH]{};GetModuleFileNameW(nullptr,p,MAX_PATH);LoadLibraryW((std::filesystem::path(p).parent_path()/L"xinput1_3.dll").c_str());ShowWindow(h,SW_SHOW);MSG msg{};while(msg.message!=WM_QUIT){if(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}else{float color[4]={.18f,.29f,.32f,1};ctx->OMSetRenderTargets(1,&rt,nullptr);ctx->ClearRenderTargetView(rt,color);sc->Present(1,0);}}return 0;}

