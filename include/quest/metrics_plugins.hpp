#pragma once
#include "quest/plugins.hpp"
#include <json.hpp>

namespace quest {
using PluginJson=nlohmann::json;
struct MetricDescriptor { const char* id; const char* title; const char* group; };
const std::vector<MetricDescriptor>& hudMetrics();
bool configurablePlugin(const Plugin& plugin);
PluginJson pluginDefaults(const Plugin& plugin);
PluginJson pluginSettingsSchema(const Plugin& plugin);
PluginJson pluginControlActions(const Plugin& plugin);
PluginJson validatePluginSettings(const Plugin& plugin,const PluginJson& values);
JobRequest metricsPluginJob(const Plugin& plugin,const std::string& action,const PluginJson& values=PluginJson::object());
PluginJson metricsPluginResult(const Plugin& plugin,const std::string& action,const std::string& output,const PluginJson& requested=PluginJson::object(),PluginJson* capture=nullptr);
JobRequest cyInsightBootstrap(const std::wstring& jar);
}
