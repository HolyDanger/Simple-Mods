#include "bridge.h"
#include "MinHook.h"
#include "localization.h"
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
namespace Bridge {
std::atomic<bool> compatible=false,mouseEnabled=true,managerOpen=false;
using U=uintptr_t;
using PE=void(__fastcall*)(U,U,void*);
PE original=nullptr; U base=0,names=0,objects=0,menu=0,menuClass=0,startMenu=0,startClass=0,slate=0,gameplay=0,textLib=0,buttonClass=0,textClass=0,spacerClass=0; int objectCount=0;
U popup=0,inputPopup=0;std::unordered_set<int> popupNames;int inputPopupName=-1;
std::mutex lock; NativeMenu snapshot;
std::atomic<int64_t> requested=-1;
std::atomic<bool> modFocus=false;
std::atomic<bool> deferredActivation=false;
void(*logMessage)(const char*)=nullptr;
template<class T>T read(U a){__try{return a?*reinterpret_cast<T*>(a):T{};}__except(EXCEPTION_EXECUTE_HANDLER){return T{};}}
bool copy(void* dest,U source,size_t n){__try{memcpy(dest,(void*)source,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
std::string name(int i){int total=read<int>(names+1024);if(i<0||i>=total)return {};U c=read<U>(names+8*(i/16384)),e=read<U>(c+8*(i%16384));char s[256]{}; if(!copy(s,e+16,255))return {};return s;}
std::string objectName(U o){return name(read<int>(o+24));}
U object(int i){U c=read<U>(objects+8*(i/66560));return read<U>(c+24*(i%66560));}
bool valid(U o){int i=read<int>(o+12);return o&&i>=0&&i<read<int>(base+0x30e3f88+20)&&object(i)==o&&!(read<uint32_t>(o+8)&(0x10|0x20|0x8000|0x10000));}
U field(U cl,const char* nm){thread_local std::unordered_map<U,std::unordered_map<std::string,U>> cache;U key=cl;auto& entries=cache[key];auto found=entries.find(nm);if(found!=entries.end())return found->second;for(int level=0;cl&&level<16;level++,cl=read<U>(cl+0x30)){for(U p=read<U>(cl+0x38),j=0;p&&j<2000;p=read<U>(p+0x28),j++){if(objectName(p)==nm){entries[nm]=p;return p;}}}return 0;}
int offset(U o,const char* nm){U p=field(read<U>(o+16),nm);int n=read<int>(p+0x44);return p&&n>=0&&n<0x20000?n:-1;}
U prop(U o,const char* nm){int off=offset(o,nm);return off>=0?read<U>(o+off):0;}
U fn(U o,const char* nm){return field(read<U>(o+16),nm);}
void call(U o,const char* nm,void* params){U f=fn(o,nm);if(f)original(o,f,params);}
int po(U f,const char* nm){U p=field(f,nm);return p?read<int>(p+0x44):-1;}
bool geometry(U widget,NativeRect& r){
    U gf=fn(widget,"GetCachedGeometry"),lf=fn(slate,"GetLocalSize"),vf=fn(slate,"LocalToViewport");
    if(!gf||!lf||!vf)return false;
    alignas(16) unsigned char g[256]{},l[256]{},v[256]{};
    original(widget,gf,g);int gr=po(gf,"ReturnValue"),lg=po(lf,"Geometry"),lr=po(lf,"ReturnValue"),vg=po(vf,"Geometry"),wc=po(vf,"WorldContextObject"),lc=po(vf,"LocalCoordinate"),px=po(vf,"PixelPosition");
    if(gr<0||lg<0||lr<0||vg<0||wc<0||lc<0||px<0)return false;
    U gp=field(gf,"ReturnValue"),st=read<U>(gp+0x70);int sz=read<int>(st+0x40);if(sz<16||sz>128)return false;
    memcpy(l+lg,g+gr,sz);original(slate,lf,l);float size[2];memcpy(size,l+lr,8);
    if(size[0]<1||size[1]<1||size[0]>20000||size[1]>20000)return false;
    memcpy(v+vg,g+gr,sz);memcpy(v+wc,&widget,8);original(slate,vf,v);float a[2];memcpy(a,v+px,8);
    memcpy(v+lc,size,8);original(slate,vf,v);float b[2];memcpy(b,v+px,8);
    r.x=a[0];r.y=a[1];r.w=b[0]-a[0];r.h=b[1]-a[1];return r.w>1&&r.h>1;
}
U changedPC=0;int changedOffset=-1;unsigned char oldBit=0,mask=0;
void restoreMouse(){if(changedPC&&valid(changedPC)&&changedOffset>=0){auto& b=*(unsigned char*)(changedPC+changedOffset);b=(b&~mask)|oldBit;}changedPC=0;changedOffset=-1;}
void cursor(U m,bool enable){
    if(!enable){restoreMouse();return;}U pc=0;call(m,"GetOwningPlayer",&pc);if(!valid(pc))return;
    if(changedPC==pc){*(unsigned char*)(pc+changedOffset)|=mask;return;}restoreMouse();U p=field(read<U>(pc+16),"bShowMouseCursor");if(!p)return;
    int off=read<int>(p+0x44)+read<unsigned char>(p+0x71);unsigned char fm=read<unsigned char>(p+0x73);if(off<=0||!fm)return;
    changedPC=pc;changedOffset=off;mask=fm;oldBit=read<unsigned char>(pc+off)&fm;*(unsigned char*)(pc+off)|=fm;
}
thread_local bool nested=false; ULONGLONG lastUpdate=0;
struct Translation { U widget; float value[2]; };
U layoutMenu=0;std::vector<Translation> moved;
U pauseModButton=0,pauseModOwner=0,pauseModText=0,frontLabel=0;
alignas(16) unsigned char modsText[2][40]{};bool textReady=false;
bool makeText(const wchar_t* value,unsigned char* buffer){
    U f=fn(textLib,"Conv_StringToText");int in=po(f,"inString"),out=po(f,"ReturnValue");
    if(!f||in!=0||out!=16)return false;
    struct String{const wchar_t*data;int count,capacity;}str{value,(int)wcslen(value)+1,(int)wcslen(value)+1};
    memcpy(buffer,&str,sizeof str);original(textLib,f,buffer);return true;
}
bool prepareText(){
    if(textReady)return true;
    if(!textLib)return false;
    textReady=makeText(L"MODS",modsText[0])&&makeText(L"МОДЫ",modsText[1]);return textReady;
}
bool setModsText(U widget){
    if(!valid(widget)||!prepareText())return false;
    call(widget,"SetText",modsText[L10n::russian.load()?1:0]+16);return true;
}
void detectLanguage(U m){
    static ULONGLONG checked=0;auto now=GetTickCount64();if(now-checked<500)return;checked=now;
    int language=offset(m,"CurLanguage");
    if(language>=0){
        bool ru=read<unsigned char>(m+language)==1,before=L10n::russian.exchange(ru);
        static bool reported=false;if((before!=ru||!reported)&&logMessage){reported=true;logMessage(ru?"Interface language: Russian (Hello Neighbor settings)":"Interface language: English (Hello Neighbor settings)");}
    }
    U front=prop(startMenu,"TB_Modding");if(valid(front))setModsText(front);
    if(valid(pauseModText)&&valid(pauseModButton))setModsText(pauseModText);
}
U spawn(U cls,U outer){alignas(16) U args[3]{cls,outer,0};call(gameplay,"SpawnObject",args);return args[2];}
U add(U parent,U widget,const char* function){U args[2]{widget,0};call(parent,function,args);return args[1];}
void copySetting(U target,U source,const char* property,const char* setter){int off=offset(source,property);if(off>=0)call(target,setter,(void*)(source+off));}
void createPauseMod(U m,const std::vector<U>& buttons){
    if(buttons.size()!=8||pauseModOwner==m||!gameplay||!buttonClass||!textClass)return;
    U settings=buttons[5],slot=prop(settings,"Slot"),parent=prop(slot,"Parent"),tree=prop(m,"WidgetTree");
    if(!parent||objectName(read<U>(parent+16))!="VerticalBox")return;
    U b=spawn(buttonClass,tree),text=spawn(textClass,tree);if(!valid(b)||!valid(text))return;
    U sourceText=prop(m,"TB_Settings");copySetting(text,sourceText,"Font","SetFont");copySetting(text,sourceText,"ColorAndOpacity","SetColorAndOpacity");setModsText(text);
    U textSlot=add(b,text,"AddChild");U oldTextSlot=prop(sourceText,"Slot");
    for(auto setting:{std::pair<const char*,const char*>{"Padding","SetPadding"},{"HorizontalAlignment","SetHorizontalAlignment"},{"VerticalAlignment","SetVerticalAlignment"}})copySetting(textSlot,oldTextSlot,setting.first,setting.second);
    copySetting(b,settings,"WidgetStyle","SetStyle");
    struct SlotState{U widget;unsigned char padding[16],size[8];unsigned char h,v;};std::vector<SlotState> states;
    int so=offset(parent,"Slots");if(so<0)return;U slots=read<U>(parent+so);int total=read<int>(parent+so+8);int settingsIndex=-1;U spacer=0;SlotState reference{};
    for(int i=0;i<total&&i<100;i++){U s=read<U>(slots+i*8),w=prop(s,"Content");SlotState st{};st.widget=w;int p=offset(s,"Padding"),z=offset(s,"Size"),h=offset(s,"HorizontalAlignment"),v=offset(s,"VerticalAlignment");if(p>=0)copy(st.padding,s+p,16);if(z>=0)copy(st.size,s+z,8);st.h=h>=0?read<unsigned char>(s+h):0;st.v=v>=0?read<unsigned char>(s+v):0;if(w==settings){settingsIndex=(int)states.size();reference=st;}if(objectName(read<U>(w+16))=="Spacer")spacer=w;states.push_back(st);}
    if(settingsIndex<0)return;SlotState extra=reference;extra.widget=b;
    std::vector<SlotState> inserted;
    if(spacer&&spacerClass){U copySpacer=spawn(spacerClass,tree);copySetting(copySpacer,spacer,"Size","SetSize");auto it=std::find_if(states.begin(),states.end(),[&](const SlotState&st){return st.widget==spacer;});if(it!=states.end()){SlotState spacing=*it;spacing.widget=copySpacer;inserted.push_back(spacing);}}
    else if(spacerClass&&settingsIndex+1<(int)states.size()&&states[settingsIndex+1].widget!=buttons[6]){
        U gapWidget=states[settingsIndex+1].widget;U gf=fn(gapWidget,"GetCachedGeometry"),lf=fn(slate,"GetLocalSize");alignas(16) unsigned char g[256]{},l[256]{};if(gf&&lf){original(gapWidget,gf,g);int gr=po(gf,"ReturnValue"),lg=po(lf,"Geometry"),lr=po(lf,"ReturnValue");if(gr>=0&&lg>=0&&lr>=0){memcpy(l+lg,g+gr,56);original(slate,lf,l);U gap=spawn(spacerClass,tree);call(gap,"SetSize",l+lr);SlotState spacing=states[settingsIndex+1];spacing.widget=gap;inserted.push_back(spacing);}}
    }
    inserted.push_back(extra);states.insert(states.begin()+settingsIndex+1,inserted.begin(),inserted.end());
    call(parent,"ClearChildren",nullptr);
    for(auto&st:states){U s=add(parent,st.widget,"AddChildToVerticalBox");call(s,"SetPadding",st.padding);call(s,"SetSize",st.size);call(s,"SetHorizontalAlignment",&st.h);call(s,"SetVerticalAlignment",&st.v);}
    pauseModButton=b;pauseModOwner=m;pauseModText=text;if(logMessage)logMessage("Native pause Mods button created with the Settings font and slot layout.");
}
bool extendBackground(U widget,const NativeRect&first,const NativeRect&last,float gap,int depth=0){
    if(!widget||depth>30)return false;
    if(objectName(read<U>(widget+16))=="Image"){
        static int diagnosticCount=0;NativeRect diagnostic{};if(diagnosticCount++<12&&geometry(widget,diagnostic)&&logMessage){char line[256];sprintf_s(line,"Pause background %s %.1f %.1f %.1f %.1f gap=%.1f",objectName(widget).c_str(),diagnostic.x,diagnostic.y,diagnostic.w,diagnostic.h,gap);logMessage(line);}
        NativeRect r{};if((objectName(widget)=="Image"&&read<int>(widget+28)==5)&&geometry(widget,r)){
            int off=offset(widget,"RenderTransform");float scale[2]{1,1};if(off>=0)copy(scale,widget+off+8,8);scale[1]*=1+gap*1.5f/r.h;
            float pivot[2]{};call(widget,"SetRenderTransformPivot",pivot);call(widget,"SetRenderScale",scale);return true;
        }
    }
    int off=offset(widget,"Slots");if(off<0)return false;U slots=read<U>(widget+off);int count=read<int>(widget+off+8);bool changed=false;for(int i=0;i<count&&i<200;i++)changed=extendBackground(prop(read<U>(slots+i*8),"Content"),first,last,gap,depth+1)||changed;return changed;
}
void applyStyle(U button,U source){
    U f=fn(button,"SetStyle");if(!f||!source)return;U p=field(f,"InStyle"),st=read<U>(p+0x70);int size=read<int>(st+0x40),off=read<int>(p+0x44);
    alignas(16) unsigned char args[4096]{};if(p&&size>0&&off>=0&&size+off<4096&&copy(args+off,source,size))original(button,f,args);
}
void layout(U m,const std::vector<U>& buttons,NativeMenu&s){
    if(s.frontend){U label=prop(m,"TB_Modding");if(label&&label!=frontLabel){setModsText(label);frontLabel=label;}
        for(int i=0;i<(int)buttons.size();i++)if(objectName(buttons[i])=="BT_Modding"){s.modIndex=i;geometry(buttons[i],s.mods);s.mods.index=i;break;}return;}
    if(buttons.size()<8)return;
    NativeRect setting{},save{},previous{};
    if(!geometry(buttons[5],setting)||!geometry(buttons[6],save)||!geometry(buttons[4],previous))return;
    float gap=setting.y-previous.y;if(gap<8||gap>300)return;
    s.rowGap=gap;createPauseMod(m,buttons);if(valid(pauseModButton)&&pauseModOwner==m)geometry(pauseModButton,s.mods);s.mods.index=-1;
    if(layoutMenu!=m){
        moved.clear();
        NativeRect first{},last{};if(geometry(buttons[0],first)&&geometry(buttons[7],last)&&extendBackground(prop(m,"ScreenMenuPause"),first,last,gap))layoutMenu=m;
    }
}
bool visibleWidget(U widget){for(int i=0;widget&&i<40;i++){
    auto v=read<unsigned char>(widget+0xb7);if(v==1||v==2)return false;
    U slot=prop(widget,"Slot");if(!slot)break;widget=prop(slot,"Parent");
}return true;}
bool displayed(U w){if(!valid(w)||!visibleWidget(w))return false;
    U current=w;for(int i=0;current&&i<12;i++){
        bool viewport=false;if(fn(current,"IsInViewport"))call(current,"IsInViewport",&viewport);if(viewport)return true;
        U slot=prop(current,"Slot"),parent=prop(slot,"Parent");if(parent){current=parent;if(!visibleWidget(current))return false;continue;}
        U outer=read<U>(current+32);if(objectName(read<U>(outer+16))=="WidgetTree")current=read<U>(outer+32);else break;
    }return false;
}
U hiddenOwner=0;bool hiding=false;std::vector<std::pair<U,unsigned char>> hiddenWidgets;
void hideNative(U m,bool hide){
    if(hiding&&(hiddenOwner!=m||!hide)){for(auto p:hiddenWidgets)if(valid(p.first))call(p.first,"SetVisibility",&p.second);hiddenWidgets.clear();hiding=false;}
    if(hide&&!hiding&&valid(m)){hiddenOwner=m;for(const char*nm:{"ScreenMenuPause","ScreenSettings","ScreenActs","ScreenSave","ScreenLoad","BPW_NavigationMenu"}){U w=prop(m,nm);if(valid(w)){auto v=read<unsigned char>(w+0xb7);hiddenWidgets.push_back({w,v});unsigned char hidden=2;call(w,"SetVisibility",&hidden);}}hiding=true;}
}
void buttonGroup(U m,int screen,U& data,int& count,U& selector){
    const char* arrays[]={"ButtonsMenuPause","ButtonsMenuSettings","ButtonsSubMenuControl","ButtonsSubMenuGraphics","ButtonsSubMenuLanguage","ButtonsSubMenuMixer","ButtonsSubMenuNeighbor","ButtonsMenuActs"};
    const char* selectors[]={"SelectorMenuPause","SelectorMenuSettings","SelectorSubMenuControl","SelectorSubMenuGraphics","SelectorSubMenuLanguage","SelectorSubMenuMixer","SelectorSubMenuNeighbor","SelectorMenuActs"};
    if(screen>=0&&screen<8){int a=offset(m,arrays[screen]),s=offset(m,selectors[screen]);if(a>=0){data=read<U>(m+a);count=read<int>(m+a+8);}if(s>=0)selector=m+s;return;}
    const char* nm=screen==9?"ButtonsSaveSlot":screen==10?"ButtonsLoadSlot":screen==11?"ButtonsActions":nullptr;
    if(!nm)return;U p=field(read<U>(m+16),nm),st=read<U>(p+0x70);int off=read<int>(p+0x44);
    for(U child=read<U>(st+0x38),j=0;child&&j<64;child=read<U>(child+0x28),j++){
        auto n=objectName(child);int innerOff=read<int>(child+0x44);
        if(n.rfind("Buttons_",0)==0&&objectName(read<U>(child+16))=="ArrayProperty"){data=read<U>(m+off+innerOff);count=read<int>(m+off+innerOff+8);}
        if(n.rfind("Selector_",0)==0)selector=m+off+innerOff;
    }
}
U sliderFor(U m,U button){auto n=objectName(button);
    if(n=="BT_InvertAxisY")return prop(m,"Slider_InvertMouseY");
    if(n=="BT_NeighborFriendly")return prop(m,"Slider_FriendlyMode");
    if(n.rfind("BT_",0)==0)return prop(m,("Slider_"+n.substr(3)).c_str());return 0;
}
void adjustNative(U m,bool right){
    int off=offset(m,"CurScreen");int screen=off>=0?read<unsigned char>(m+off):-1;
    unsigned char key=right?5:4;
    const char* update=screen==3?"UpdateButtonGraphics":screen==2?"UpdateButtonControls":screen==5?"UpdateButtonMixer":nullptr;
    if(update)call(m,update,&key);else call(m,right?"OnPressRight":"OnPressLeft",nullptr);
}
void setRange(U m,U slider,float desired){
    int valueOffset=offset(slider,"Value");if(valueOffset<0)return;
    float target=std::clamp(desired,0.f,1.f);
    for(int i=0;i<64;i++){
        float before=read<float>(slider+valueOffset);call(slider,"GetValue",&before);
        if(std::abs(before-target)<.0001f)break;
        bool right=target>before;adjustNative(m,right);
        float after=read<float>(slider+valueOffset);call(slider,"GetValue",&after);
        if(std::abs(after-before)<.000001f)break;
        if((right&&after>=target)||(!right&&after<=target)){
            if(std::abs(before-target)<std::abs(after-target))adjustNative(m,!right);
            break;
        }
    }
}
void appendRows(U m,int screen,int indexBase,NativeMenu& s){
    U data=0,selector=0;int count=0;buttonGroup(m,screen,data,count,selector);
    for(int i=0;data&&i<count&&count<256;i++){
        U b=read<U>(data+i*8);if(!b||!visibleWidget(b))continue;
        NativeRect r{};r.index=indexBase+i;auto nm=objectName(b);
        r.adjustable=screen==3||nm=="BT_SensivityMouse"||nm=="BT_InvertAxisY"||nm=="BT_FriendlyMode";
        U slider=sliderFor(m,b);NativeRect sr{};
        if(slider&&geometry(slider,sr)){r.sx=sr.x;r.sy=sr.y;r.sw=sr.w;r.sh=sr.h;}
        if(geometry(b,r))s.buttons.push_back(r);
    }
}
void update(){
    NativeMenu s;U m=displayed(menu)?menu:displayed(startMenu)?startMenu:0,owner=m;std::vector<U> buttons;U selector=0;bool modal=false;
    s.frontend=m&&m==startMenu;
    if(valid(m)){
        detectLanguage(m);
        s.visible=displayed(m);
        if(s.visible){int co=offset(m,"CurScreen");s.screen=co>=0?read<unsigned char>(m+co):-1;s.main=s.screen==0;
            U data=0;int count=0;buttonGroup(m,s.screen,data,count,selector);s.selected=selector?read<int>(selector):0;
            if(data&&count>0&&count<256){s.count=count;for(int i=0;i<count;i++)buttons.push_back(read<U>(data+i*8));
                if(s.main)layout(m,buttons,s);
                appendRows(m,s.screen,0,s);
            }
            if(s.screen==1){int screens[]={2,4,6,5,3};if(s.selected>=0&&s.selected<5)appendRows(m,screens[s.selected],256,s);}
            else if(s.screen>=2&&s.screen<=6)appendRows(m,1,512,s);
            U nav=prop(m,"BPW_NavigationMenu");NativeRect nr{};
            if(nav&&visibleWidget(nav)){for(const char* nm:{"TB_Back1","TB_Back2","TB_Back3"}){U text=prop(nav,nm);if(text&&visibleWidget(text)&&geometry(text,nr)){s.back=nr;s.back.x-=nr.h*1.7f;s.back.w+=nr.h*1.7f;break;}}}
        }
    }
    if(valid(popup)){
        if(displayed(popup)){owner=popup;modal=true;s.visible=true;s.main=false;s.screen=100;s.buttons.clear();s.mods={};s.back={};buttons={prop(popup,"BT_Yes"),prop(popup,"BT_No")};s.count=2;int off=offset(popup,"ActiveButton");selector=off>=0?popup+off:0;s.selected=read<U>(selector)==buttons[0]?0:1;for(int i=0;i<2;i++){NativeRect r{};r.index=i;if(buttons[i]&&visibleWidget(buttons[i])&&geometry(buttons[i],r))s.buttons.push_back(r);}}
    }
    if(displayed(inputPopup)){s.capture=true;s.buttons.clear();}
    hideNative(m,s.visible&&managerOpen.load());
    cursor(m,s.visible&&(mouseEnabled.load()||managerOpen.load()));
    int64_t packed=requested.exchange(-1);int index=packed<0?-1:(int)(packed&0xffff);bool go=packed>=0&&(packed&0x10000);
    if(packed>=0&&s.visible&&(packed>>32)==s.screen){
        if(!modal&&index>=256&&index<768){
            if(!(packed&0xf0000))index=-1;
            else {
                if(index>=512){
                    index-=512;call(m,"OnPressBack",nullptr);
                    U data=0,sel=0;int count=0;buttonGroup(m,1,data,count,sel);
                    int current=sel?read<int>(sel):0;
                    for(int i=0;i<std::abs(index-current);i++)call(m,index>current?"OnPressDown":"OnPressUp",nullptr);
                    call(m,"OnPressEnter",nullptr);index=-1;
                }else{
                    index-=256;call(m,"OnPressEnter",nullptr);
                    int co=offset(m,"CurScreen");s.screen=co>=0?read<unsigned char>(m+co):-1;
                    U data=0;int count=0;buttonGroup(m,s.screen,data,count,selector);
                    buttons.clear();for(int i=0;data&&i<count&&count<256;i++)buttons.push_back(read<U>(data+i*8));
                    s.count=(int)buttons.size();s.selected=selector?read<int>(selector):0;
                }
            }
        }
        if(index==0xfffe)call(owner,modal?"PressBack":"OnPressBack",nullptr);
        else if(index>=0&&index<s.count){
            if(modal){if(index!=s.selected)call(owner,index==0?"PressLeft":"PressRight",nullptr);if(go)deferredActivation=true;}
            else {int diff=index-s.selected;int n=(std::min)(std::abs(diff),256);for(int i=0;i<n;i++)call(m,diff>0?"OnPressDown":"OnPressUp",nullptr);if(packed&0x80000)setRange(m,sliderFor(m,buttons[index]),(float)((packed>>20)&4095)/4095.f);else if(packed&0x20000)adjustNative(m,true);else if(packed&0x40000)adjustNative(m,false);else if(go)call(m,"OnPressEnter",nullptr);}
        }
    }
    if(selector)s.selected=modal?(read<U>(selector)==buttons[0]?0:1):read<int>(selector);
    static bool wasModFocus=false;
    bool focus=s.visible&&s.main&&modFocus.load();
    if(s.main&&!s.frontend&&s.selected>=0&&s.selected<(int)buttons.size()&&(focus||wasModFocus!=focus)){
        int styleOff=offset(m,focus?"UnSelectStyleLong":"SelectStyleLong");if(styleOff>=0)applyStyle(buttons[s.selected],m+styleOff);
    }wasModFocus=focus;
    if(s.main&&!s.frontend&&pauseModOwner==m&&valid(pauseModButton)){int off=offset(m,focus?"SelectStyleLong":"UnSelectStyleLong");if(off>=0)applyStyle(pauseModButton,m+off);}
    static int reported=-2;if(s.visible&&s.screen!=reported){reported=s.screen;char b[128];sprintf_s(b,"Menu screen=%d, buttons=%d, pointer regions=%zu",s.screen,s.count,s.buttons.size());if(logMessage)logMessage(b);}
    std::lock_guard<std::mutex> guard(lock);snapshot=std::move(s);
}
void __fastcall hook(U self,U function,void* params){
    original(self,function,params);
    if(nested)return;
    U cl=read<U>(self+16);int id=read<int>(cl+24);if(popupNames.count(id)&&valid(self))popup=self;if(id==inputPopupName&&valid(self))inputPopup=self;
    if(read<U>(self+16)==menuClass&&valid(self)&&prop(self,"GameInstance")){menu=self;}
    if(cl==startClass&&valid(self)&&prop(self,"GameInstance"))startMenu=self;
    auto now=GetTickCount64();if(now-lastUpdate<30)return;
    DWORD tid=GetCurrentThreadId();static DWORD gameThread=0;
    if(self==menu||self==startMenu||self==popup)gameThread=tid;if(!gameThread||tid!=gameThread)return;
    nested=true;lastUpdate=now;update();nested=false;
}
bool Init(void(*logger)(const char*)){
    logMessage=logger;base=(U)GetModuleHandleW(nullptr);auto dos=(IMAGE_DOS_HEADER*)base;auto nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);
    if(nt->FileHeader.TimeDateStamp!=1545572128||nt->OptionalHeader.SizeOfImage!=55848960){logger("Native menu: unsupported executable profile; F6 manager remains available.");return false;}
    names=read<U>(base+0x30db2d0);objects=read<U>(base+0x30e3f88);objectCount=read<int>(base+0x30e3f88+20);
    if(name(0)!="None"||name(1)!="ByteProperty"||objectCount<1000||objectCount>2000000){logger("Native menu: object validation failed.");return false;}
    for(int i=0;i<objectCount;i++){U o=object(i);auto n=objectName(o),type=objectName(read<U>(o+16));if(n=="Default__SlateBlueprintLibrary")slate=o;if(n=="Default__GameplayStatics")gameplay=o;if(n=="Default__KismetTextLibrary")textLib=o;if(n=="Button"&&type=="Class")buttonClass=o;if(n=="TextBlock"&&type=="Class")textClass=o;if(n=="Spacer"&&type=="Class")spacerClass=o;if(n=="BPW_MenuPause_C"&&type=="WidgetBlueprintGeneratedClass")menuClass=o;if(n=="BPW_StartGame_C"&&type=="WidgetBlueprintGeneratedClass")startClass=o;if(read<U>(o+16)==menuClass&&valid(o)&&prop(o,"GameInstance"))menu=o;if(read<U>(o+16)==startClass&&valid(o)&&prop(o,"GameInstance"))startMenu=o;}
    int total=read<int>(names+1024);for(int i=0;i<total;i++){auto n=name(i);if(n=="BPW_RequestYesNo_C"||n=="BPW_RequestQuit_C"||n=="BPW_RequestRestartAct_C")popupNames.insert(i);if(n=="BPW_EnterKey_C")inputPopupName=i;}
    unsigned char sig[]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};unsigned char got[sizeof sig]{};
    if(!menuClass)return false;
    if(!slate||!copy(got,base+0x8049d0,sizeof got)||memcmp(sig,got,sizeof sig)){logger("Native menu: ProcessEvent signature validation failed.");return false;}
    if(MH_CreateHook((void*)(base+0x8049d0),(void*)hook,(void**)&original)!=MH_OK||MH_EnableHook((void*)(base+0x8049d0))!=MH_OK){logger("Native menu: could not install callback.");return false;}
    compatible=true;logger("Native menu bridge ready (GOG profile, reflected geometry).");return true;
}
NativeMenu Snapshot(){std::lock_guard<std::mutex>g(lock);return snapshot;}
void queue(int index,uint32_t flags){int screen=Snapshot().screen;if(screen<0)return;int64_t value=((int64_t)screen<<32)|(index&0xffff)|(int64_t)flags;
    if(flags||index==0xfffe){requested=value;return;}
    auto previous=requested.load();do{if(previous>=0&&((previous&0xf0000)||(previous&0xffff)==0xfffe))return;}while(!requested.compare_exchange_weak(previous,value));
}
void Select(int index,bool go){queue(index,go?0x10000:0);}
void Adjust(int index,int direction){queue(index,direction>0?0x20000:0x40000);}
void Range(int index,float value){uint32_t quantized=(uint32_t)(std::clamp(value,0.f,1.f)*4095.f+.5f);queue(index,0x80000|(quantized<<20));}
void Back(){queue(0xfffe,0);}
bool TakeActivation(){return deferredActivation.exchange(false);}
void SetModFocus(bool value){modFocus=value;}
void SetMouse(bool value){mouseEnabled=value;}
}








