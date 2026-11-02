// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/log_view.hpp"
#include "quest/core.hpp"
#include <cyui/modules/CodeEditor.h>
#include <TextEditor.h>
#include <imgui_internal.h>
namespace quest {
struct TextView::Impl {cyui::CodeEditor editor;std::string source;float previous{-1};ImGuiID window{};int language{-1};Impl(){editor.setReadOnly(true);auto& native=editor.native();native.SetShowMiniMapEnabled(false);native.SetShowScrollbarMiniMapEnabled(false);native.SetShowLineNumbersEnabled(false);native.SetShowWhitespacesEnabled(false);native.SetShowCurrentLineHighlightEnabled(false);}};
TextView::TextView():impl_(std::make_unique<Impl>()){} TextView::~TextView()=default;
TextView::TextView(TextView&&) noexcept=default;TextView& TextView::operator=(TextView&&) noexcept=default;
bool TextView::selected()const{return impl_->editor.native().AnyCursorHasSelection();}
bool TextView::focused()const{return GImGui&&GImGui->NavWindow&&GImGui->NavWindow->ID==impl_->window;}
void TextView::clearSelection(){auto& native=impl_->editor.native();native.SetCursor(native.GetMainCursorPosition());}
void TextView::focus(){impl_->editor.native().SetFocus();}
std::string TextView::text()const{return impl_->editor.text();}
void followLogViewport(bool& follow,bool paused,float& previous,float tolerance){
    auto y=ImGui::GetScrollY();bool up=ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)&&ImGui::GetIO().MouseWheel>0;
    follow=logFollow(follow,previous,y,ImGui::GetScrollMaxY(),up,tolerance);previous=y;
    if(follow&&!paused)ImGui::SetScrollFromPosY(ImGui::GetCursorPosY(),1.f);
}
bool TextView::draw(const char* label,const std::string& text,ImVec2 size,bool& follow,bool paused,int language,float followTolerance){
    auto* parent=ImGui::GetCurrentWindow();auto id=ImGui::GetID(label);auto& v=*impl_;auto& native=v.editor.native();
    if(followTolerance<0)followTolerance=cyui::dp(2);
    if(auto* child=ImGui::FindWindowByID(v.window)){bool up=GImGui->HoveredWindow==child&&ImGui::GetIO().MouseWheel>0;float y=child->Scroll.y;if(child->ScrollTarget.y<FLT_MAX&&child->ScrollTargetCenterRatio.y==0)y=std::clamp(child->ScrollTarget.y,0.f,child->ScrollMax.y);follow=logFollow(follow,v.previous,y,child->ScrollMax.y,up,followTolerance);}
    if(selected())follow=false;
    if(v.language!=language){v.editor.setLanguage(language==1?cyui::CodeLanguage::json:cyui::CodeLanguage::plain);v.language=language;}
    if(v.source!=text&&!selected()){auto first=native.GetFirstVisibleRow();v.editor.setText(text);v.source=text;if(!follow)native.ScrollToLine(first,TextEditor::Scroll::alignTop);}
    if(follow&&!paused&&native.GetLineCount())native.ScrollToLine(native.GetLineCount()-1,TextEditor::Scroll::alignBottom);
    v.editor.draw(label,size);bool selection=selected();
    for(auto* child:parent->DC.ChildWindows)if(child->ChildId==id){
        v.window=child->ID;v.previous=child->Scroll.y;
        if(selection)follow=false;
        break;
    }
    return selection;
}
bool drawRawLog(TextView& view,const std::string& text,bool& follow,bool paused,float& previous,ImVec2 size,float tolerance){(void)previous;return view.draw("##rawLog",text,size,follow,paused,0,tolerance);}
}
