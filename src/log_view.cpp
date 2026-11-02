// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/log_view.hpp"
#include "quest/core.hpp"
#include <imgui_internal.h>
namespace quest {
bool rawLogSelection(){auto* state=ImGui::GetInputTextState(ImGui::GetID("##rawLog"));return state&&state->HasSelection();}
void clearRawLogSelection(){auto* state=ImGui::GetInputTextState(ImGui::GetID("##rawLog"));if(state)state->ClearSelection();}
void followLogViewport(bool& follow,bool paused,float& previous,float tolerance){
    auto y=ImGui::GetScrollY();bool up=ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)&&ImGui::GetIO().MouseWheel>0;
    follow=logFollow(follow,previous,y,ImGui::GetScrollMaxY(),up,tolerance);previous=y;
    if(follow&&!paused)ImGui::SetScrollFromPosY(ImGui::GetCursorPosY(),1.f);
}
bool drawRawLog(std::string& text,bool& follow,bool paused,float& previous,ImVec2 size,float tolerance){
    auto* parent=ImGui::GetCurrentWindow();auto id=ImGui::GetID("##rawLog");
    ImGui::InputTextMultiline("##rawLog",text.data(),text.size()+1,size,ImGuiInputTextFlags_ReadOnly);
    auto* state=ImGui::GetInputTextState(id);bool selected=state&&state->HasSelection();
    for(auto* child:parent->DC.ChildWindows)if(child->ChildId==id){
        bool up=ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)&&ImGui::GetIO().MouseWheel>0;
        follow=logFollow(follow,previous,child->Scroll.y,child->ScrollMax.y,up,tolerance);previous=child->Scroll.y;
        if(selected)follow=false;
        if(follow&&!paused)ImGui::SetScrollY(child,child->DC.CursorMaxPos.y-child->Pos.y+child->Scroll.y);
        break;
    }
    return selected;
}
}
