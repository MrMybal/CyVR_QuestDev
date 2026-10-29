// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "quest/backend.hpp"
#include <filesystem>

namespace quest {
struct PluginAction { std::string id,label,command; int timeout{30000}; };
struct Plugin {
    std::string id,name,version,description,package,error,controller;
    std::filesystem::path path;
    int localPort{},remotePort{};
    bool reverse{},enabled{true};
    std::vector<PluginAction> actions;
};
Plugin parsePlugin(const std::string& text);
std::vector<Plugin> loadPlugins(const std::vector<std::filesystem::path>& roots);
JobRequest pluginLink(const Plugin& plugin,bool disconnect=false);
JobRequest pluginAction(const Plugin& plugin,const PluginAction& action);
}
