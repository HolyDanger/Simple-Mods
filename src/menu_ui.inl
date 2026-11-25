static const std::string& localized(const std::string& en, const std::string& ru) {
 return L10n::russian && !ru.empty() ? ru : en;
}

static void drawFields(ImDrawList* draw, Mod& mod, float x, float y, float scale) {
 const int count = static_cast<int>(mod.fields.size());
 const int rows = 4;
 auto& io = ImGui::GetIO();
 if (hit(x, y, 535 * scale, 182 * scale) && io.MouseWheel != 0)
  fieldIndex = std::clamp(fieldIndex + (io.MouseWheel > 0 ? -1 : 1), 0, count - 1);
 fieldTop = std::clamp(fieldTop, std::max(0, fieldIndex - rows + 1), fieldIndex);
 for (int i = fieldTop; i < std::min(count, fieldTop + rows); ++i) {
  auto& field = mod.fields[i];
  const float row = y + (i - fieldTop) * 45 * scale;
  if (i == fieldIndex) quad(draw, x, row, 530 * scale, 40 * scale, IM_COL32(0,0,0,160), 6 * scale);
  if (hit(x, row, 530 * scale, 40 * scale) && ImGui::IsMouseClicked(0)) fieldIndex = i;
  const auto label = utf8(L10n::russian && !field.labelRu.empty() ? field.labelRu : field.label);
  draw->PushClipRect({x, row}, {x + 230 * scale, row + 40 * scale});
  txt(draw, x + 12 * scale, row + 8 * scale, 20 * scale, IM_COL32_WHITE, label.c_str());
  draw->PopClipRect();
  if (button(draw, x + 232 * scale, row, 42 * scale, 38 * scale, "-")) adjustSetting(mod, field, -1);
  if (button(draw, x + 425 * scale, row, 42 * scale, 38 * scale, "+")) adjustSetting(mod, field, 1);
  if (button(draw, x + 469 * scale, row, 67 * scale, 38 * scale, L10n::Text(L10n::Key::Reset)))
   changeSetting(mod, field, field.fallback);
  std::string value = std::to_string(field.value);
  if (field.kind == Manifest::Kind::Toggle)
   value = L10n::Text(field.value ? L10n::Key::Enabled : L10n::Key::Disabled);
  else if (field.kind == Manifest::Kind::Choice) {
   const auto& choices = L10n::russian && field.choicesRu.size() == field.choices.size() ?
      field.choicesRu : field.choices;
   value = utf8(choices[field.value]);
  }
  draw->PushClipRect({x + 275 * scale, row}, {x + 425 * scale, row + 40 * scale});
  txt(draw, x + 281 * scale, row + 8 * scale, 19 * scale, IM_COL32_WHITE, value.c_str());
  draw->PopClipRect();
 }
}

static void ui(const NativeMenu& menu){
 static int dragging=-1,dragScreen=-1;
 auto begin=std::chrono::steady_clock::now();auto&io=ImGui::GetIO();float W=io.DisplaySize.x,H=io.DisplaySize.y;auto*d=ImGui::GetBackgroundDrawList();
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
  quad(d,x-14*sc,y,pw*.55f,78*sc,IM_COL32(0,0,0,255),15*sc);txt(d,x+30*sc,y+10*sc,43*sc,IM_COL32_WHITE,L10n::Text(L10n::Key::Mods));
  int visibleRows=4;modListTop=std::clamp(modListTop,0,std::max(0,(int)mods.size()-visibleRows));
  if(activeMod<modListTop)modListTop=activeMod;if(activeMod>=modListTop+visibleRows)modListTop=activeMod-visibleRows+1;
  if(hit(x+12*sc,y+114*sc,300*sc,320*sc)&&io.MouseWheel!=0){modListTop=std::clamp(modListTop-(io.MouseWheel>0?1:-1),0,std::max(0,(int)mods.size()-visibleRows));int selected=std::clamp(activeMod,modListTop,std::min((int)mods.size()-1,modListTop+visibleRows-1));if(selected!=activeMod){activeMod=selected;openSettings(false);}}
  for(int i=modListTop;i<(int)mods.size()&&i<modListTop+visibleRows;i++){
   auto&m=mods[i];float yy=y+118*sc+(i-modListTop)*78*sc;bool over=hit(x+16*sc,yy,295*sc,68*sc);
   if(i==activeMod||over)quad(d,x+16*sc,yy,295*sc,68*sc,IM_COL32(0,0,0,i==activeMod?230:120),10*sc);
   d->PushClipRect({x+38*sc,yy},{x+308*sc,yy+68*sc});
   txt(d,x+40*sc,yy+4*sc,27*sc,IM_COL32_WHITE,localized(m.name,m.nameRu).c_str());txt(d,x+41*sc,yy+39*sc,17*sc,IM_COL32(221,216,207,255),L10n::Text(m.enabled?L10n::Key::Enabled:L10n::Key::Disabled));
   d->PopClipRect();
   if(over&&ImGui::IsMouseClicked(0)&&activeMod!=i){activeMod=i;openSettings(false);}
  }
  auto&m=mods[activeMod];float rx=x+365*sc;
  d->PushClipRect({rx,y+118*sc},{rx+535*sc,y+178*sc});
  txt(d,rx,y+118*sc,32*sc,IM_COL32_WHITE,localized(m.name,m.nameRu).c_str());
  const auto metadata = m.author.empty() ? m.version : m.version + "  ·  " + m.author;
  txt(d,rx,y+155*sc,16*sc,IM_COL32(221,216,207,255),metadata.c_str());
  d->PopClipRect();
  if(settings&&m.builtin){txt(d,rx,y+190*sc,24*sc,IM_COL32_WHITE,L10n::Text(L10n::Key::Controls));
   if(button(d,rx,y+241*sc,245*sc,47*sc,L10n::Text(L10n::Key::Mouse),mouseMode)){mouseMode=true;saveCursor();}
   if(button(d,rx+250*sc,y+241*sc,280*sc,47*sc,L10n::Text(L10n::Key::Keyboard),!mouseMode)){mouseMode=false;saveCursor();}
  }else if(settings && !m.fields.empty()){drawFields(d,m,rx,y+183*sc,sc);}
  else if(settings){txt(d,rx,y+191*sc,21*sc,IM_COL32_WHITE,L10n::Text(L10n::Key::NoSettings));}
  else {
   d->PushClipRect({rx,y+185*sc},{rx+535*sc,y+365*sc});
   d->AddText(font,23*sc,{rx,y+188*sc},IM_COL32_WHITE,m.builtin?L10n::Text(L10n::Key::CursorDescription):localized(m.description,m.descriptionRu).c_str(),nullptr,535*sc);
   d->PopClipRect();
  }
  auto errorKey=m.error;
  if(errorKey==L10n::Key::None && !m.builtin && safeMode)errorKey=L10n::Key::SafeMode;
  if(errorKey==L10n::Key::None && m.manifestWarning)errorKey=L10n::Key::InvalidManifest;
  if(errorKey!=L10n::Key::None){
   std::string error=L10n::Text(errorKey)+m.detail;
   d->PushClipRect({rx,y+368*sc},{rx+535*sc,y+430*sc});
   d->AddText(font,18*sc,{rx,y+369*sc},IM_COL32(255,175,147,255),error.c_str(),nullptr,535*sc);
   d->PopClipRect();
  }
  if(button(d,rx,y+ph-102*sc,235*sc,48*sc,L10n::Text(m.enabled?L10n::Key::Disable:L10n::Key::Enable)))enable(m,!m.enabled);
  if(button(d,rx+250*sc,y+ph-102*sc,285*sc,48*sc,L10n::Text(settings?L10n::Key::Description:L10n::Key::Settings)))openSettings(!settings);
  if(button(d,x+16*sc,y+ph-102*sc,295*sc,48*sc,L10n::Text(L10n::Key::Back)))toggleOpen();
 }
 io.MouseDrawCursor=open||(menu.visible&&cursorEnabled&&mouseMode);
 frameMs=frameMs*.95+std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()*.05;
}


