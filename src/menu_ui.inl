static void ui(){
 static int dragging=-1,dragScreen=-1;
 auto begin=std::chrono::steady_clock::now();auto&io=ImGui::GetIO();float W=io.DisplaySize.x,H=io.DisplaySize.y;auto*d=ImGui::GetBackgroundDrawList();auto menu=Bridge::Snapshot();
 if(!menu.visible){modsSelected=false;Bridge::SetModFocus(false);}
 if(!open&&menu.visible){
  if(menu.main&&menu.mods.w>1){auto&r=menu.mods;bool over=hit(r.x,r.y,r.w,r.h);
   if(over&&ImGui::IsMouseClicked(0)){modsSelected=!menu.frontend;Bridge::SetModFocus(!menu.frontend);toggleOpen();}
   if(over&&cursorEnabled&&mouseMode){if(menu.frontend){Bridge::Select(menu.modIndex,false);}else{modsSelected=true;Bridge::SetModFocus(true);}}
  }
  if(cursorEnabled&&mouseMode){
   if(!ImGui::IsMouseDown(0)||dragScreen!=menu.screen)dragging=-1;
   if(dragging>=0)for(auto&r:menu.buttons)if(r.index==dragging&&r.sw>0)Bridge::Range(r.index,(io.MousePos.x-r.sx)/r.sw);
   int hovered=-1;for(auto&r:menu.buttons)if(hit(r.x,r.y,r.w,r.h)||(r.sw>0&&hit(r.sx,r.sy,r.sw,r.sh))){
    hovered=r.index;modsSelected=false;Bridge::SetModFocus(false);
    if(hovered<256&&hovered!=menu.selected)Bridge::Select(hovered,false);
    if(ImGui::IsMouseClicked(0)&&!(menu.main&&menu.frontend&&hovered==menu.modIndex)){if(r.sw>0&&hit(r.sx,r.sy,r.sw,r.sh)){dragging=r.index;dragScreen=menu.screen;Bridge::Range(r.index,(io.MousePos.x-r.sx)/r.sw);}else if(r.adjustable){if(r.sw<=0)Bridge::Adjust(hovered,1);}else Bridge::Select(hovered,true);}
    if(r.adjustable&&io.MouseWheel!=0)Bridge::Adjust(hovered,io.MouseWheel>0?1:-1);
    break;
   }
   if(ImGui::IsMouseClicked(1))Bridge::Back();
   else if(hovered<0&&!menu.main&&menu.back.w>1&&hit(menu.back.x,menu.back.y,menu.back.w,menu.back.h)&&ImGui::IsMouseClicked(0))Bridge::Back();
   hoveredNative=hovered;
  }
 }
 if(open){
  float sc=std::min(W/1280.f,H/720.f);float pw=1000*sc,ph=535*sc,x=(W-pw)*.5f,y=(H-ph)*.5f;
  d->AddRectFilled({0,0},{W,H},IM_COL32(0,0,0,48));
  d->AddQuadFilled({x+12*sc,y+84*sc},{x+pw,y+66*sc},{x+pw-6*sc,y+ph-52*sc},{x,y+ph},IM_COL32(31,26,22,164));
  quad(d,x-14*sc,y,pw*.55f,78*sc,IM_COL32(0,0,0,255),15*sc);txt(d,x+30*sc,y+10*sc,43*sc,IM_COL32_WHITE,"Моды");
  int visibleRows=4;modListTop=std::clamp(modListTop,0,std::max(0,(int)mods.size()-visibleRows));
  if(activeMod<modListTop)modListTop=activeMod;if(activeMod>=modListTop+visibleRows)modListTop=activeMod-visibleRows+1;
  if(hit(x+12*sc,y+114*sc,300*sc,320*sc)&&io.MouseWheel!=0){modListTop=std::clamp(modListTop-(io.MouseWheel>0?1:-1),0,std::max(0,(int)mods.size()-visibleRows));activeMod=std::clamp(activeMod,modListTop,std::min((int)mods.size()-1,modListTop+visibleRows-1));}
  for(int i=modListTop;i<(int)mods.size()&&i<modListTop+visibleRows;i++){
   auto&m=mods[i];float yy=y+118*sc+(i-modListTop)*78*sc;bool over=hit(x+16*sc,yy,295*sc,68*sc);
   if(i==activeMod||over)quad(d,x+16*sc,yy,295*sc,68*sc,IM_COL32(0,0,0,i==activeMod?230:120),10*sc);
   txt(d,x+40*sc,yy+4*sc,27*sc,IM_COL32_WHITE,m.name.c_str());txt(d,x+41*sc,yy+39*sc,17*sc,IM_COL32(221,216,207,255),m.enabled?"Включён":"Выключен");
   if(over&&ImGui::IsMouseClicked(0)){activeMod=i;settings=false;}
  }
  auto&m=mods[activeMod];float rx=x+365*sc;txt(d,rx,y+118*sc,32*sc,IM_COL32_WHITE,m.name.c_str());
  if(settings&&m.builtin){txt(d,rx,y+190*sc,24*sc,IM_COL32_WHITE,"Управление");
   if(button(d,rx,y+241*sc,245*sc,47*sc,"Мышь",mouseMode)){mouseMode=true;save();}
   if(button(d,rx+250*sc,y+241*sc,280*sc,47*sc,"Клавиатура",!mouseMode)){mouseMode=false;save();}
  }else if(settings){txt(d,rx,y+191*sc,21*sc,IM_COL32_WHITE,"Дополнительных настроек нет.");}
  else d->AddText(font,23*sc,{rx,y+188*sc},IM_COL32_WHITE,m.description.c_str(),nullptr,535*sc);
  if(!m.error.empty())d->AddText(font,18*sc,{rx,y+340*sc},IM_COL32(255,175,147,255),m.error.c_str(),nullptr,535*sc);
  if(button(d,rx,y+ph-102*sc,235*sc,48*sc,m.enabled?"Выключить":"Включить"))enable(m,!m.enabled);
  if(button(d,rx+250*sc,y+ph-102*sc,285*sc,48*sc,settings?"Описание":"Настройки"))settings=!settings;
  if(button(d,x+16*sc,y+ph-102*sc,295*sc,48*sc,"Назад"))toggleOpen();
 }
 io.MouseDrawCursor=open||(menu.visible&&cursorEnabled&&mouseMode);
 frameMs=frameMs*.95+std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()*.05;
}


