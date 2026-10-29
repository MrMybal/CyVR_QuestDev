// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/plugins.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace quest {
static bool identifier(const std::string& s){return !s.empty()&&s.size()<100&&std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isalnum(c)||c=='.'||c=='-'||c=='_';});}
static int port(const std::string& s){size_t used=0;int n=std::stoi(s,&used);if(used!=s.size()||n<1||n>65535)throw std::runtime_error("Port TCP invalide.");return n;}
Plugin parsePlugin(const std::string& text) {
    Plugin p;std::string section,line;std::istringstream lines(text);int api=0;
    try {
        if(text.size()>128*1024)throw std::runtime_error("Manifest trop volumineux.");
        while(std::getline(lines,line)) {
            line=trim(line);if(line.empty()||line[0]=='#'||line[0]==';')continue;
            if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);if(section.rfind("action.",0)==0){if(p.actions.size()>=32)throw std::runtime_error("Maximum 32 actions.");p.actions.push_back({section.substr(7),"","",30000});}continue;}
            auto equal=line.find('=');if(equal==std::string::npos)throw std::runtime_error("Ligne de manifest sans '='.");
            auto key=trim(line.substr(0,equal)),value=trim(line.substr(equal+1));
            if(section=="plugin") {
                if(key=="api"){size_t used=0;api=std::stoi(value,&used);if(used!=value.size())throw std::runtime_error("API invalide.");}else if(key=="id")p.id=value;else if(key=="name")p.name=value;else if(key=="version")p.version=value;else if(key=="description")p.description=value;else if(key=="package")p.package=value;else if(key=="controller")p.controller=value;
            } else if(section=="link") {
                if(key=="local_port")p.localPort=port(value);else if(key=="remote_port")p.remotePort=port(value);else if(key=="direction"){if(value!="forward"&&value!="reverse")throw std::runtime_error("Direction attendue : forward ou reverse.");p.reverse=value=="reverse";}
            } else if(section.rfind("action.",0)==0 && !p.actions.empty()) {
                auto& a=p.actions.back();if(key=="label")a.label=value;else if(key=="shell")a.command=value;else if(key=="timeout_ms"){a.timeout=std::stoi(value);if(a.timeout<100||a.timeout>300000)throw std::runtime_error("Délai d’action hors limites.");}
            } else throw std::runtime_error("Section de manifest inconnue.");
        }
        if(api!=1)throw std::runtime_error("Version d’API non prise en charge (api=1 requis).");
        if(!identifier(p.id)||p.name.empty()||!validPackage(p.package))throw std::runtime_error("Identifiant, nom ou package invalide.");
        if(!p.controller.empty()&&!((p.controller=="ovr_metrics"&&p.package=="com.oculus.ovrmonitormetricsservice")||(p.controller=="cyinsight_ovr"&&p.package=="org.cyinsights.ovr")))throw std::runtime_error("Contrôleur ou package incompatible.");
        if((p.localPort==0)!=(p.remotePort==0))throw std::runtime_error("Les deux ports du lien sont requis.");
        std::set<std::string> ids;for(const auto& a:p.actions)if(!identifier(a.id)||a.label.empty()||a.command.empty()||!ids.insert(a.id).second)throw std::runtime_error("Action incomplète ou identifiant dupliqué.");
    } catch(const std::exception& e){p.error=e.what();p.enabled=false;}
    return p;
}
std::vector<Plugin> loadPlugins(const std::vector<std::filesystem::path>& roots) {
    std::map<std::string,Plugin> found;std::vector<Plugin> errors;
    for(const auto& root:roots) {
        std::error_code ec;std::filesystem::directory_iterator it(root,ec),end;
        for(;it!=end&&!ec;it.increment(ec)) {
            if(!it->is_regular_file(ec)||it->path().extension()!=L".cyplugin")continue;
            if(it->file_size(ec)>128*1024||ec)continue;
            std::ifstream in(it->path(),std::ios::binary);std::ostringstream content;content<<in.rdbuf();auto p=parsePlugin(content.str());p.path=it->path();
            if(p.error.empty())found[p.id]=std::move(p);else errors.push_back(std::move(p));
        }
    }
    std::vector<Plugin> out;for(auto& pair:found)out.push_back(std::move(pair.second));for(auto& p:errors)out.push_back(std::move(p));return out;
}
JobRequest pluginLink(const Plugin& p,bool disconnect) {
    if(!p.enabled||!p.error.empty()||p.localPort<1||p.localPort>65535||p.remotePort<1||p.remotePort>65535)throw std::runtime_error("Plugin ou ports invalides.");
    auto command=p.reverse?"reverse":"forward";auto first="tcp:"+std::to_string(p.reverse?p.remotePort:p.localPort);auto second="tcp:"+std::to_string(p.reverse?p.localPort:p.remotePort);
    JobRequest request;request.label=p.name+(disconnect?" · déconnexion":" · liaison");
    request.steps.push_back({disconnect?std::vector<std::string>{command,"--remove",first}:std::vector<std::string>{command,"--no-rebind",first,second},{}});return request;
}
JobRequest pluginAction(const Plugin& p,const PluginAction& a) {
    if(!p.enabled||!p.error.empty()||!validPackage(p.package))throw std::runtime_error("Plugin invalide ou désactivé.");
    auto command=a.command;auto replace=[&](const std::string& key,const std::string& value){size_t at=0;while((at=command.find(key,at))!=std::string::npos){command.replace(at,key.size(),value);at+=value.size();}};
    replace("{package}",shellQuote(p.package));replace("{local_port}",std::to_string(p.localPort));replace("{remote_port}",std::to_string(p.remotePort));
    JobRequest request;request.label=p.name+" · "+a.label;request.timeoutMs=a.timeout;request.steps.push_back({{"shell",command},{}});return request;
}
}
