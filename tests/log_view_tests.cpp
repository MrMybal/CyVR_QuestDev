// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/log_view.hpp"
#include <imgui_internal.h>
#include <iostream>
#include <stdexcept>
using namespace quest;
static int checks{};
static void check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
int main(){try{
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(800,600);io.DeltaTime=1.f/60;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    std::string clipboard;auto& platform=ImGui::GetPlatformIO();platform.Platform_ClipboardUserData=&clipboard;
    platform.Platform_SetClipboardTextFn=[](ImGuiContext* context,const char* text){*static_cast<std::string*>(context->PlatformIO.Platform_ClipboardUserData)=text;};
    platform.Platform_GetClipboardTextFn=[](ImGuiContext* context){return static_cast<std::string*>(context->PlatformIO.Platform_ClipboardUserData)->c_str();};
    TextView editor;bool follow=true,raw=true;float previous=-1;ImGuiWindow* child{};ImGuiID input{};std::string text;
    for(int i=0;i<200;++i)text+="1700000000.123  501 502 I RawTag: selectable line "+std::to_string(i)+"\n";
    auto frame=[&]{
        ImGui::NewFrame();ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(800,600));ImGui::Begin("test",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDecoration);
        auto* parent=ImGui::GetCurrentWindow();
        if(raw){input=ImGui::GetID("##rawLog");drawRawLog(editor,text,follow,false,previous,ImVec2(780,550),2);for(auto* w:parent->DC.ChildWindows)if(w->ChildId==input)child=w;}
        else if(ImGui::BeginTable("log",1,ImGuiTableFlags_ScrollY,ImVec2(780,550))){for(int i=0;i<200;++i){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Text("line %d",i);}child=ImGui::GetCurrentWindow();followLogViewport(follow,false,previous,2);ImGui::EndTable();}
        ImGui::End();ImGui::Render();
    };
    auto scroll=[&](float y){ImGui::SetScrollY(child,y);frame();frame();};
    for(int mode=0;mode<2;++mode){raw=mode==0;follow=true;previous=-1;for(int i=0;i<4;++i)frame();check(child&&child->ScrollMax.y>0,"scrollable log viewport exists");check(child->Scroll.y>=child->ScrollMax.y-2,"initial follow reaches bottom");scroll(200);check(!follow,"manual scroll suspends follow in actual ImGui viewport");scroll(child->ScrollMax.y);check(follow,"return to bottom resumes follow in actual ImGui viewport");if(raw){for(int i=0;i<20;++i)text+="new live row\n";frame();frame();frame();check(follow&&child->Scroll.y>=child->ScrollMax.y-2,"RAW keeps bottom pinned when new rows arrive");}}
    raw=true;follow=true;previous=-1;frame();frame();
    io.AddMousePosEvent(30,30);io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    editor.focus();frame();check(editor.text()==text,"CyUI RAW preserves original text bytes");
    io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();io.AddKeyEvent(ImGuiKey_A,false);frame();
    check(editor.selected()&&!follow,"Ctrl+A selects RAW and suspends tail movement");
    io.AddKeyEvent(ImGuiKey_C,true);frame();io.AddKeyEvent(ImGuiKey_C,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
    check(clipboard==text,"Ctrl+C copies exact multiline RAW bytes to clipboard");
    check(editor.focused(),"native text focus remains visible to the host shortcut guard");
    io.AddInputCharacter('X');frame();check(editor.text()==text,"read-only RAW cannot edit the source");
    auto original=text;text+="another arriving row\n";frame();check(editor.text()==original,"selection freezes its original bytes during capture updates");
    editor.clearSelection();frame();check(editor.text()==text,"clearing selection resumes the source snapshot");
    ImGui::DestroyContext();std::cout<<checks<<" log viewport checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<"\n";return 1;}}
