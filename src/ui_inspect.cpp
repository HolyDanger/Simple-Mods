#define main probe_main
#include "probe.cpp"
#undef main
uintptr_t fieldAt(uintptr_t cl,const char* n){for(int i=0;cl&&i<12;i++,cl=val<uintptr_t>(cl+0x30))for(auto p=val<uintptr_t>(cl+0x38);p;p=val<uintptr_t>(p+0x28))if(oname(p)==n)return p;return 0;}
uintptr_t getProp(uintptr_t o,const char*n){auto p=fieldAt(val<uintptr_t>(o+16),n);return p?val<uintptr_t>(o+val<int>(p+0x44)):0;}
int main(int argc,char**argv){int r=probe_main(argc,argv);if(r)return r;
 for(int i=0;i<os;i++){auto o=obj(i);if(!o||val<int>(o+8)&0x10)continue;auto cl=val<uintptr_t>(o+16);auto n=oname(cl);
 if(n=="BPW_NavigationMenu_C"||n=="BPW_StartGame_C"||n=="BP_WidgetStartMenu_C"||n=="BPW_MenuPause_C"||n=="BPW_RequestQuit_C"||n=="BPW_RequestYesNo_C"){
  printf("UI %s ptr=%llx idx=%d flags=%x\n",n.c_str(),(long long)o,i,val<int>(o+8));
  for(auto p=val<uintptr_t>(cl+0x38);p;p=val<uintptr_t>(p+0x28)){auto nm=oname(p),ty=oname(val<uintptr_t>(p+16));int off=val<int>(p+0x44);if(ty=="ObjectProperty"){
   auto w=val<uintptr_t>(o+off),slot=getProp(w,"Slot"),par=getProp(slot,"Parent");printf(" %s =%llx class=%s vis=%d slot=%s parent=%s\n",nm.c_str(),(long long)w,oname(val<uintptr_t>(w+16)).c_str(),val<unsigned char>(w+0xb7),oname(val<uintptr_t>(slot+16)).c_str(),oname(val<uintptr_t>(par+16)).c_str());
  }else if(ty=="ArrayProperty")printf(" %s ARRAY +%x count=%d\n",nm.c_str(),off,val<int>(o+off+8));else if(ty=="IntProperty"||ty=="ByteProperty"||ty=="BoolProperty")printf(" %s %s +%x value=%d\n",nm.c_str(),ty.c_str(),off,val<int>(o+off));}
 }
 if(n=="Function"&&(oname(o)=="SpawnObject"||oname(o)=="SetFont"||oname(o)=="SetLayout"||oname(o)=="SetText"||oname(o)=="Conv_StringToText")){
  printf("API %s.%s size=%d\n",oname(val<uintptr_t>(o+32)).c_str(),oname(o).c_str(),val<int>(o+0x40));for(auto p=val<uintptr_t>(o+0x38);p;p=val<uintptr_t>(p+0x28)){auto st=val<uintptr_t>(p+0x70);printf(" %s off=%x size=%d struct=%s\n",oname(p).c_str(),val<int>(p+0x44),val<int>(p+0x34),oname(st).c_str());}
 }
 }
}


