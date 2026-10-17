#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <filesystem>
#include <cstdio>
int wmain(int argc,wchar_t**argv){
 if(argc!=2){puts("NeighborAttach <absolute path to loader DLL>");return 1;}auto path=std::filesystem::absolute(argv[1]);if(!std::filesystem::is_regular_file(path)){puts("DLL not found");return 2;}
 DWORD pid=0;int matches=0;HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W p{sizeof p};if(Process32FirstW(s,&p))do{if(!_wcsicmp(p.szExeFile,L"HelloNeighbor-Win64-Shipping.exe")){pid=p.th32ProcessID;matches++;}}while(Process32NextW(s,&p));CloseHandle(s);if(matches!=1){puts("Expected exactly one running HelloNeighbor-Win64-Shipping.exe");return 3;}
 HANDLE proc=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);if(!proc){printf("OpenProcess failed: %lu\n",GetLastError());return 4;}
 HMODULE modules[2048];DWORD needed=0;EnumProcessModules(proc,modules,sizeof modules,&needed);for(unsigned i=0;i<needed/sizeof(HMODULE)&&i<2048;i++){wchar_t mp[32768]{};GetModuleFileNameExW(proc,modules[i],mp,32768);if(!_wcsicmp(mp,path.c_str())){puts("Loader already attached");CloseHandle(proc);return 0;}}
 auto text=path.wstring();size_t bytes=(text.size()+1)*sizeof(wchar_t);void* remote=VirtualAllocEx(proc,nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!remote||!WriteProcessMemory(proc,remote,text.c_str(),bytes,nullptr)){puts("Could not copy loader path");if(remote)VirtualFreeEx(proc,remote,0,MEM_RELEASE);CloseHandle(proc);return 5;}
 auto load=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");HMODULE owner=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)load,&owner);wchar_t ownerPath[MAX_PATH]{};GetModuleFileNameW(owner,ownerPath,MAX_PATH);auto ownerName=std::filesystem::path(ownerPath).filename().wstring();uintptr_t remoteBase=0;
 for(unsigned i=0;i<needed/sizeof(HMODULE)&&i<2048;i++){wchar_t mp[MAX_PATH]{};GetModuleBaseNameW(proc,modules[i],mp,MAX_PATH);if(!_wcsicmp(mp,ownerName.c_str())){remoteBase=(uintptr_t)modules[i];break;}}
 if(!remoteBase){puts("Could not resolve remote Windows module");VirtualFreeEx(proc,remote,0,MEM_RELEASE);CloseHandle(proc);return 6;}
 auto entry=(LPTHREAD_START_ROUTINE)(remoteBase+(uintptr_t)load-(uintptr_t)owner);HANDLE thread=CreateRemoteThread(proc,nullptr,0,entry,remote,0,nullptr);if(!thread){printf("CreateRemoteThread failed: %lu\n",GetLastError());VirtualFreeEx(proc,remote,0,MEM_RELEASE);CloseHandle(proc);return 7;}
 auto wait=WaitForSingleObject(thread,15000);DWORD result=0;if(wait==WAIT_OBJECT_0){GetExitCodeThread(thread,&result);VirtualFreeEx(proc,remote,0,MEM_RELEASE);}CloseHandle(thread);CloseHandle(proc);if(wait!=WAIT_OBJECT_0){puts("Attach timed out; path retained until process exits");return 8;}printf("LoadLibrary result: %lx (check SimpleMods.log for runtime status)\n",result);return result?0:9;
}

