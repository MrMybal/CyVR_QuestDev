// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <imgui.h>
#include <string>
namespace quest {
bool rawLogSelection();
void clearRawLogSelection();
bool drawRawLog(std::string& text,bool& follow,bool paused,float& previous,ImVec2 size,float tolerance);
void followLogViewport(bool& follow,bool paused,float& previous,float tolerance);
}
