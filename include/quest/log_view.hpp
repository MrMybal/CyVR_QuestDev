// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <imgui.h>
#include <string>
#include <memory>
namespace quest {
// Host adapter only: the editor, selection, syntax, clipboard and horizontal
// scrolling are supplied by CyUI::CodeEditor. No C++20 leaks into the host ABI.
class TextView {
public:
    TextView();~TextView();TextView(TextView&&) noexcept;TextView& operator=(TextView&&) noexcept;
    bool selected() const;bool focused() const;void clearSelection();void focus();
    bool draw(const char* id,const std::string& text,ImVec2 size,bool& follow,bool paused=false,int language=0,float followTolerance=-1.f);
    std::string text() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
bool drawRawLog(TextView& view,const std::string& text,bool& follow,bool paused,float& previous,ImVec2 size,float tolerance);
void followLogViewport(bool& follow,bool paused,float& previous,float tolerance);
}
