#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
HANDLE hp; uintptr_t base;
bool read(uintptr_t a,void* p,size_t n){SIZE_T z=0;return ReadProcessMemory(hp,(void*)a,p,n,&z)&&z==n;}
template<class T>T val(uintptr_t a){T v{};read(a,&v,sizeof v);return v;}
uintptr_t names=0,objects=0;int ns=0,os=0,stride=24;bool chunked=false;int nameoff=16;
std::string name(int i){if(i<0||i>=ns)return "?"; auto c=val<uintptr_t>(names+8*(i/16384));auto e=val<uintptr_t>(c+8*(i%16384));char t[256]{};read(e+nameoff,t,255);return t;}
std::string oname(uintptr_t o){return name(val<int>(o+24));}
uintptr_t obj(int i){auto c=chunked?val<uintptr_t>(objects+8*(i/66560)):objects;return val<uintptr_t>(c+stride*(chunked?(i%66560):i));}
void props(uintptr_t c){for(int level=0;c&&level<8;level++,c=val<uintptr_t>(c+0x30)){printf(" CLASS %s @%llx\n",oname(c).c_str(),(long long)c); auto p=val<uintptr_t>(c+0x38);for(int j=0;p&&j<400;j++,p=val<uintptr_t>(p+0x28)){auto n=oname(p);int off=val<int>(p+0x44);printf("  %s +%x type=%s\n",n.c_str(),off,oname(val<uintptr_t>(p+16)).c_str());}}}
int main(int argc,char**argv){DWORD pid=argc>1?strtoul(argv[1],nullptr,10):0; if(!pid){auto s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W p{sizeof p};if(Process32FirstW(s,&p))do{if(!_wcsicmp(p.szExeFile,L"HelloNeighbor-Win64-Shipping.exe")){pid=p.th32ProcessID;break;}}while(Process32NextW(s,&p));CloseHandle(s);} hp=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,pid);if(!hp)return 1; auto s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);MODULEENTRY32W m{sizeof m};Module32FirstW(s,&m);base=(uintptr_t)m.modBaseAddr;CloseHandle(s);printf("pid=%lu base=%llx\n",pid,(long long)base);auto nt=base+val<LONG>(base+0x3c);auto count=val<WORD>(nt+6);auto sec=nt+24+val<WORD>(nt+20);
for(int k=0;k<count;k++){IMAGE_SECTION_HEADER h{};read(sec+k*sizeof h,&h,sizeof h);if(!(h.Characteristics&IMAGE_SCN_MEM_WRITE))continue; auto start=base+h.VirtualAddress;std::vector<unsigned char>b(h.Misc.VirtualSize);if(!read(start,b.data(),b.size()))continue;printf("section %.8s %x\n",h.Name,h.Misc.VirtualSize);
for(size_t i=0;i+1040<b.size();i+=8){for(int indirect=0;indirect<2;indirect++){auto p=indirect?*(uintptr_t*)&b[i]:start+i; unsigned char temp[1040]{}; if(indirect){if(p<0x10000000000||p>=base||p%8||!read(p,temp,sizeof temp))continue;}auto data=indirect?temp:&b[i];int num=*(int*)&data[1024],chunks=*(int*)&data[1028];if(num>1000&&num<2000000&&chunks==(num+16383)/16384){auto c=*(uintptr_t*)data;auto e=val<uintptr_t>(c);for(int off:{12,16}){char t[5]{};if(read(e+off,t,5)&&!memcmp(t,"None",5)){names=p;ns=num;nameoff=off;printf("NAMES rva=%llx num=%d off=%d indirect=%d\n",(long long)(start+i-base),num,off,indirect);}}}}}
}
if(!names){puts("no names");return 2;}printf("name1=%s\n",name(1).c_str());
for(int k=0;k<count;k++){IMAGE_SECTION_HEADER h{};read(sec+k*sizeof h,&h,sizeof h);if(!(h.Characteristics&IMAGE_SCN_MEM_WRITE))continue; auto start=base+h.VirtualAddress;std::vector<unsigned char>b(h.Misc.VirtualSize);if(!read(start,b.data(),b.size()))continue;for(size_t i=0;i+32<b.size();i+=8){for(int mode=0;mode<2;mode++){int num=*(int*)&b[i+(mode?20:12)],max=*(int*)&b[i+(mode?16:8)];if(num<10000||num>2000000||max<num||max>4000000)continue;auto ar=*(uintptr_t*)&b[i];auto first=mode?val<uintptr_t>(ar):ar;auto o=val<uintptr_t>(first);auto vt=val<uintptr_t>(o);if(vt<base||vt>base+m.modBaseSize)continue;int ni=val<int>(o+24);if(ni<0||ni>=ns)continue;auto cl=val<uintptr_t>(o+16);if(oname(cl)!="Package"&&oname(cl)!="Class")continue;objects=ar;os=num;chunked=mode;printf("OBJECTS rva=%llx num=%d chunked=%d first=%s class=%s\n",(long long)(start+i-base),num,mode,oname(o).c_str(),oname(cl).c_str());goto found;}}}
found: if(!objects){puts("no objects");return 3;}
for(int i=0;i<os;i++){auto o=obj(i);if(!o)continue;auto n=oname(o);auto cl=oname(val<uintptr_t>(o+16));
if(cl=="Function"&&(n=="GetCachedGeometry"||n=="LocalToViewport"||n=="GetLocalSize"||n=="IsInViewport"||n=="OnPressEnter")){printf("FUNCTION %s outer=%s size=%d\n",n.c_str(),oname(val<uintptr_t>(o+32)).c_str(),val<int>(o+0x40));for(auto p=val<uintptr_t>(o+0x38);p;p=val<uintptr_t>(p+0x28)){auto st=val<uintptr_t>(p+0x70);printf(" %s off=%x elsize=%d struct=%s sz=%d\n",oname(p).c_str(),val<int>(p+0x44),val<int>(p+0x34),oname(st).c_str(),val<int>(st+0x40));}}
if(cl=="BPW_MenuPause_C"&&!(val<int>(o+8)&16)){printf("INSTANCE %llx vis=%d screen=%d selector=%d gi=%llx\n",(long long)o,val<unsigned char>(o+0xb7),val<unsigned char>(o+0x11d0),val<int>(o+0x7c8),(long long)val<uintptr_t>(o+0x11f0));auto data=val<uintptr_t>(o+0x758);int n=val<int>(o+0x760);for(int a=0;a<n&&a<20;a++){auto b=val<uintptr_t>(data+a*8);printf(" BUTTON[%d] %s type=%s vis=%d\n",a,oname(b).c_str(),oname(val<uintptr_t>(b+16)).c_str(),val<unsigned char>(b+0xb7));}}
}
return 0;
}

