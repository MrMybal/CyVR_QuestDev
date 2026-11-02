// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/app.hpp"
#include "quest/plugins.hpp"
#include "quest/preview.hpp"
#include "quest/assistant_bridge.hpp"
#include "quest/files.hpp"
#include "quest/file_workspace.hpp"
#include "quest/metrics_plugins.hpp"
#include "quest/metrics_view.hpp"
#include "quest/log_view.hpp"
#include "quest/mirroring.hpp"
#include "quest/resources.h"
#include "quest/version.hpp"
#include <cyui/CyUI.h>
#include <imgui.h>
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace quest {
namespace {
std::wstring fileDialog(bool save, const wchar_t* filter, const wchar_t* extension, const wchar_t* initial = L"") {
    wchar_t path[32768]{}; wcsncpy_s(path,initial,_TRUNCATE);
    OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=GetActiveWindow();ofn.lpstrFilter=filter;ofn.lpstrFile=path;ofn.nMaxFile=32768;ofn.lpstrDefExt=extension;
    ofn.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    return (save?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn)) ? path : L"";
}
std::wstring folderDialog() {
    IFileOpenDialog* dialog{};std::wstring path;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){
        dialog->SetOptions(FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);
        if(SUCCEEDED(dialog->Show(GetActiveWindow()))){IShellItem* item{};if(SUCCEEDED(dialog->GetResult(&item))){PWSTR value{};if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&value))){path=value;CoTaskMemFree(value);}item->Release();}}
        dialog->Release();
    }return path;
}
std::string metricsCommand(const std::string& action){return "am broadcast -n com.oculus.ovrmonitormetricsservice/.SettingsBroadcastReceiver -a com.oculus.ovrmonitormetricsservice."+action;}
std::filesystem::path settingsPath() {
    wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    auto dir=std::filesystem::path(local)/L"CyVR/QuestDev";std::error_code ec;std::filesystem::create_directories(dir,ec);return dir/L"settings.ini";
}
bool isFile(const std::wstring& path) { std::error_code ec;return std::filesystem::is_regular_file(path,ec); }
std::filesystem::path appRoot(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return std::filesystem::path(path).parent_path();}
std::filesystem::path userRoot(){return settingsPath().parent_path();}
std::filesystem::path productRoot(){auto p=appRoot();for(int i=0;i<4;++i){if(isFile((p/L"assistant/service.py").wstring()))return p;p=p.parent_path();}return appRoot();}
void openLocal(const std::filesystem::path& path){ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
std::string shortName(const std::string& name) {
    auto i=name.rfind('.');return i==std::string::npos?name:name.substr(i+1);
}
std::string clockText(double epoch) {
    auto sec=static_cast<time_t>(epoch);tm time{};localtime_s(&time,&sec);char buf[48];
    std::snprintf(buf,sizeof(buf),"%02d:%02d:%02d.%03d",time.tm_hour,time.tm_min,time.tm_sec,int((epoch-std::floor(epoch))*1000));return buf;
}
struct View {
    std::string package;
    std::array<char,256> search{}, exclude{}, tag{};
    int minimum{0},pid{};
    bool follow{true},paused{},events{},rawMode{},rawTail{true},rawSelected{};
    uint64_t freeze{},selected{},revision{~uint64_t(0)};
    float lastScroll{-1};
    uint64_t rawRevision{~uint64_t(0)},rawAnchor{};
    std::string rawText;
    size_t rawFirst{},rawEnd{};
    std::vector<LogSort> sort{{LogColumn::time,false}};
    bool dirty{true},rawInvalidate{true};
    std::vector<uint64_t> ids;
};
struct Counts { int lines{},errors{},warnings{},processes{}; };
struct Task {uint64_t id{};std::string label,output,kind;bool done{},ok{};int code{};std::wstring path;};
struct PluginPanel {Json draft=Json::object(),observed=Json::object(),capture=Json::object();std::map<std::string,bool> available;std::string error,message,requestedCapture,controller;uint64_t task{};int group{};};
struct PluginRun {Plugin plugin;std::string action;Json settings;};
RemoteListing readListing(const std::wstring& path){std::error_code ec;auto bytes=std::filesystem::file_size(path,ec);if(ec||bytes>4*1024*1024)throw std::runtime_error("Liste absente ou trop volumineuse.");std::ifstream in(path,std::ios::binary);return parseFileListing(std::string((std::istreambuf_iterator<char>(in)),{}));}
}
struct App::Impl {
    Options options;
    std::wstring adb;
    std::unique_ptr<Backend> backend;
    LogStore store;
    std::vector<Device> devices;
    std::vector<Package> packages;
    std::map<std::string,Counts> counts;
    std::string serial,status="Recherche des casques…",lastOperation;
    uint64_t session{},countRevision{~uint64_t(0)};
    int page{},appMode{};
    bool online{},capturing{},compare{},confirmStop{},detailsOpen{},tabRequest{true},initialApplied{};
    std::string pendingStop;
    std::array<char,192> appSearch{},network{};
    std::array<char,2048> adbField{};
    View view,second;
    std::vector<std::string> tabs;
    std::optional<LogEntry> detail;
    std::string iniPath;
    int applicationCategory{},deviceSection{},settingsSection{},selectedPlugin{},terminalMode{},terminalTimeout{30};
    std::array<char,192> managementSearch{},browserUrl{};
    std::array<char,8192> command{};
    std::map<uint64_t,Task> tasks;
    std::vector<uint64_t> taskOrder;
    uint64_t selectedTask{},infoTask{},recordTask{};
    std::unique_ptr<Preview> preview;
    std::unique_ptr<Preview> appLogo;
    std::unique_ptr<Mirroring> videoMirror;
    MirrorOptions mirrorDraft;
    bool mirrorSetup{},mirrorCrop{};
    std::wstring mirrorEngine;
    std::vector<MirrorTicket> mirrorUi;
    std::map<std::string,std::pair<MirrorTicket,uint64_t>> nativeMirrors;
    std::map<std::string,std::string> deviceInfo;
    std::map<std::string,bool> deviceRequests;
    std::vector<Plugin> plugins;
    std::map<std::string,bool> pluginLinked;
    std::map<std::string,PluginPanel> pluginPanels;
    std::map<uint64_t,PluginRun> pluginRuns;
    std::map<uint64_t,std::wstring> pluginCapturePaths;
    std::unique_ptr<MetricsView> metrics;
    std::unique_ptr<FileWorkspace> files;
    int logLines{50000},logMemoryMb{64};
    bool filesSplitPreference{},filesQueuePreference{};
    std::map<std::string,std::pair<MetricsTicket,uint64_t>> nativeMetrics;
    bool pluginTabRequest{true};
    bool newPlugin{},confirmDevice{};
    std::string confirmationText;
    JobRequest confirmationJob;
    std::array<char,128> pluginName{},pluginPackage{};
    int pluginLocalPort{8787},pluginRemotePort{8787},recordSeconds{30},traceSeconds{10};
    std::string recordRemote,recordPid;
    std::chrono::steady_clock::time_point nextInfo{};
    std::unique_ptr<AssistantBridge> assistant;
    std::string assistantStatus="Non démarré",mcpConfig,connectionFile;
    bool assistantReady{},assistantConnected{},assistantBusy{},sendContext{true},sendLogs{true},floatingAssistant{};
    int provider{},aiPolicy{1};
    std::array<char,256> aiModel{},aiEndpoint{},aiKeyEnv{},aiKey{};
    std::array<char,1024> aiExecutable{};
    std::array<char,8192> aiPrompt{};
    std::vector<std::pair<std::string,std::string>> chat;
    Json aiModels=Json::array();std::vector<Json> approvals;
    std::map<uint64_t,std::string> nativeJobs;
    std::map<uint64_t,Json> nativeOutputs;
    std::chrono::steady_clock::time_point nextContext{};
    std::unique_ptr<AssistantBridge> updateBridge;
    Json updateState={{"phase","idle"},{"busy",false},{"current",AppVersion},{"status","Recherchez les nouvelles versions sur GitHub."},{"prefs",{{"channel","alpha"},{"autoCheck",false},{"useGit",true}}}};
    std::array<char,2049> updateToken{};
    bool updateAutoChecked{};
    bool updateWorkerReady{};
    HANDLE updateInstaller{};
    std::filesystem::path updateReady;
    std::chrono::steady_clock::time_point updateLaunchTime{};

    explicit Impl(Options o) : options(std::move(o)),adb(options.adb.empty()?findAdb():options.adb),page(options.page),compare(options.compare) {
        if(!options.demo) {
            std::ifstream in(settingsPath());std::string line;
            while(std::getline(in,line)) {
                auto number=[&](const char* key,int& target,int minimum,int maximum){std::string prefix=std::string(key)+"=";if(line.rfind(prefix,0)!=0)return;try{size_t used=0;auto value=std::stoll(line.substr(prefix.size()),&used);if(used==line.size()-prefix.size())target=int(std::clamp(value,static_cast<long long>(minimum),static_cast<long long>(maximum)));}catch(...){} };
                number("log_lines",logLines,1000,5000000);number("log_memory_mb",logMemoryMb,16,2048);
                if(line=="files_side_by_side=1")filesSplitPreference=true;
                if(line=="files_prepare_list=1")filesQueuePreference=true;
                if(line.rfind("theme=",0)==0)cyui::setTheme(line.substr(6).c_str());
                if(line.rfind("adb=",0)==0 && options.adb.empty()) { auto saved=widen(line.substr(4));if(isFile(saved))adb=saved; }
            }
            iniPath=narrow((settingsPath().parent_path()/L"layout.ini").wstring());ImGui::GetIO().IniFilename=iniPath.c_str();
            backend=std::make_unique<Backend>(adb);
        }
        auto a=narrow(adb);strncpy_s(adbField.data(),adbField.size(),a.c_str(),_TRUNCATE);
        store.setLimits(size_t(logLines),size_t(logMemoryMb)*1024*1024);
        preview=std::make_unique<Preview>(options.graphicsDevice);reloadPlugins();
        appLogo=std::make_unique<Preview>(options.graphicsDevice);std::string logoError;if(!appLogo->loadResource(IDR_QUESTDEV_LOGO,logoError))cyui::notify(logoError.c_str(),cyui::Tone::error);
        videoMirror=std::make_unique<Mirroring>();mirrorSetup=options.mirrorSetup;
        mirrorEngine=options.scrcpy.empty()?(productRoot()/L"runtime/mirror/scrcpy.exe").wstring():options.scrcpy;
        metrics=std::make_unique<MetricsView>(userRoot(),options.demo);
        files=std::make_unique<FileWorkspace>(userRoot(),options.demo,filesSplitPreference||options.filesSplit,filesQueuePreference||options.filesQueue,
            [this](JobRequest job,const std::wstring& output){return submit(job.label,std::move(job.steps),"files:workspace",output,job.timeoutMs,std::move(job.cleanupSteps));},
            [this](uint64_t job){if(backend)backend->cancel(job);},[this]{saveSettings();});
        if(!options.metricsCsv.empty())metrics->capture().request("open_csv",options.metricsCsv);
        for(size_t i=0;i<plugins.size();++i)if(plugins[i].id==options.plugin)selectedPlugin=int(i);
        assistant=std::make_unique<AssistantBridge>();
        if(!options.demo){std::ifstream profile(userRoot()/L"ai-profile.json");Json j;try{profile>>j;provider=std::clamp(j.value("provider",0),0,6);auto copy=[&](auto& field,const char* key){strncpy_s(field.data(),field.size(),j.value(key,std::string()).c_str(),_TRUNCATE);};copy(aiModel,"model");copy(aiEndpoint,"endpoint");copy(aiKeyEnv,"api_key_env");copy(aiExecutable,"executable");}catch(...){} }
        if(options.demo){seedDemo();if(options.filesQueue)files->seedQueueDemo();}
        else if(isFile((productRoot()/L"runtime/python/Scripts/python.exe").wstring()))startAssistant();
        if(options.updatesSettings)settingsSection=1;
        if(options.logSettings)settingsSection=2;
        view.rawMode=second.rawMode=options.logRaw;view.sort=second.sort={{LogColumn(options.logSort),options.logDescending}};
        if(!options.demo){updateBridge=std::make_unique<AssistantBridge>();std::string error;if(!updateBridge->start((productRoot()/L"runtime/python/Scripts/python.exe").wstring(),(productRoot()/L"updater/service.py").wstring(),(userRoot()/L"updates").wstring(),error))updateState["error"]="Le runtime de mise à jour n’est pas disponible. Utilisez une archive portable complète.";}
    }
    ~Impl(){if(updateBridge)updateBridge->stop();if(updateInstaller)CloseHandle(updateInstaller);SecureZeroMemory(updateToken.data(),updateToken.size());videoMirror.reset();metrics.reset();backend.reset();files.reset();if(assistant)assistant->stop();}
    void saveSettings() {
        if(options.demo)return;
        std::ofstream out(settingsPath());out<<"theme="<<cyui::currentThemeKey()<<"\nadb="<<narrow(adb)<<"\n";
        out<<"log_lines="<<store.capacity()<<"\nlog_memory_mb="<<store.byteLimit()/(1024*1024)<<"\nfiles_side_by_side="<<(files&&files->sideBySide())<<"\nfiles_prepare_list="<<(files&&files->prepareList())<<"\n";
        if(!out)cyui::notify("Impossible de sauvegarder les réglages.",cyui::Tone::error);
    }
    void selectDevice(const std::string& s) {
        if(videoMirror->state().active)mirrorUi.push_back(videoMirror->stop());
        for(auto& [id,panel]:pluginPanels){panel.observed=Json::object();panel.capture=Json::object();panel.requestedCapture.clear();panel.available.clear();panel.error.clear();panel.message.clear();panel.task=0;}
        serial=s;session=backend->select(s);files->tick(serial,session,false,false);store.reset();packages.clear();tabs.clear();view=View{};second=View{};detail.reset();deviceInfo.clear();deviceRequests.clear();pluginLinked.clear();infoTask=recordTask=0;online=false;capturing=false;status="Connexion au casque…";
    }
    void seedDemo() {
        devices={{"DEMO-QUEST-3","device","Meta Quest 3"}};serial=devices[0].serial;online=true;capturing=true;status="Démonstration · données simulées";
        packages={{"com.cyberalien.cyvr.launcher",10101,false},{"com.cyberalien.cyvr.player",10102,false},{"com.cyberalien.cyvr.link",10103,false},{"com.oculus.vrshell",10050,true},{"com.android.systemui",10051,true}};
        store.setPackages(packages);
        deviceInfo={{"MODEL","Meta Quest 3 · démonstration"},{"OS","14"},{"BATTERY","82"},{"GPU","1"},{"GUARDIAN","0"},{"PROX","Virtual proximity state: ENABLED"},{"WIFI","-1"},{"METRICS","package:OVR Metrics"},{"STORAGE","256 Go · données simulées"}};
        const double epoch=1793289600;
        store.snapshot({{4218,10101,packages[0].name},{4302,10101,packages[0].name+":renderer"},{6801,10102,packages[1].name},{7230,10103,packages[2].name},{1420,10050,packages[3].name},{810,1000,"system_server"}},epoch);
        const std::vector<std::pair<char,std::string>> messages{
            {'I',"Session OpenXR initialisée · Meta Quest 3"},{'I',"Bibliothèque prête : 24 applications"},{'D',"Chargement de la scène principale"},{'I',"Rendu stéréo : 2064 × 2208 par œil"},{'D',"Frame présentée · CPU 4.2 ms / GPU 5.8 ms"},{'I',"Suivi des mains actif · fréquence 60 Hz"},{'W',"Budget GPU dépassé : 12.4 ms (cible 11.1 ms)"},{'D',"Cache de textures : 184 Mo / 512 Mo"},{'I',"Application au premier plan"},{'E',"Échec de chargement : environments/studio.ktx2"},{'I',"Environnement de secours chargé"},{'D',"Frame présentée · CPU 4.1 ms / GPU 5.6 ms"}};
        for(int i=0;i<85;++i) {
            const auto& m=messages[i%messages.size()];int pid=i%5==1?6801:i%7==2?7230:i%6==3?4302:4218;
            std::ostringstream line;line<<std::fixed<<std::setprecision(6)<<epoch+i*0.137<<" "<<pid<<" "<<pid+2<<" "<<m.first<<" "<<(pid==6801?"CyVR.Player":pid==7230?"CyVR.Link":i%3==0?"OpenXR":"CyVR.Launcher")<<": "<<m.second;store.ingest(line.str());
        }
        openApp(options.package.empty()?packages[0].name:options.package);view.follow=false;second.package=packages[1].name;
    }
    void tick() {
        tickUpdates();
        for(auto it=mirrorUi.begin();it!=mirrorUi.end();){if(it->result.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=it->result.get();if(!result.ok)cyui::notify(result.error.c_str(),cyui::Tone::error);it=mirrorUi.erase(it);}else ++it;}
        for(auto it=nativeMirrors.begin();it!=nativeMirrors.end();){if(it->second.first.result.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=it->second.first.result.get();if(it->second.second!=session){result.ok=false;result.error="Le casque sélectionné a changé.";}replyNative(it->first,result.ok,result.data,result.error);it=nativeMirrors.erase(it);}else ++it;}
        metrics->capture().device(adb,serial,session,online&&!options.demo);
        for(auto it=nativeMetrics.begin();it!=nativeMetrics.end();){if(it->second.first.result.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=it->second.first.result.get();if(it->second.second!=session){result.ok=false;result.error="Le casque sélectionné a changé.";}replyNative(it->first,result.ok,result.data,result.error);it=nativeMetrics.erase(it);}else ++it;}
        tickAssistant();
        if(!backend){files->tick(serial,session,online,page==7);return;}
        for(auto& e:backend->poll()) {
            if(e.kind==BackendEvent::Devices) {
                devices=std::move(e.devices);
                online=std::any_of(devices.begin(),devices.end(),[&](const Device& d){return d.serial==serial && d.state=="device";});
                if(!e.ok){status=e.text;capturing=false;}
                else if(serial.empty()) {
                    std::vector<Device> ready;for(const auto& d:devices)if(d.state=="device")ready.push_back(d);
                    if(ready.size()==1)selectDevice(ready[0].serial);
                    else status=devices.empty()?"Aucun casque détecté":"Sélectionnez un casque autorisé";
                }
                if(!online)capturing=false;
                continue;
            }
            if(e.session!=session){if(e.kind==BackendEvent::JobDone&&(nativeJobs.count(e.job)||pluginRuns.count(e.job)||(tasks.count(e.job)&&tasks[e.job].kind=="files:workspace")))jobEvent(e);continue;}
            if(e.kind==BackendEvent::JobStarted||e.kind==BackendEvent::JobOutput||e.kind==BackendEvent::JobDone){jobEvent(e);continue;}
            switch(e.kind) {
            case BackendEvent::Packages:
                if(e.ok){packages=std::move(e.packages);store.setPackages(packages);if(!initialApplied && !options.package.empty()){openApp(options.package);initialApplied=true;}}else cyui::notify(e.text.c_str(),cyui::Tone::error);break;
            case BackendEvent::Processes:store.snapshot(e.processes,e.epoch);break;
            case BackendEvent::Lines:for(const auto& line:e.lines)store.ingest(line);break;
            case BackendEvent::Status:status=e.text;capturing=e.ok;break;
            case BackendEvent::Operation:lastOperation=e.text;cyui::notify(e.ok?"Opération terminée":"L’opération a échoué",e.ok?cyui::Tone::success:cyui::Tone::error);break;
            default:break;
            }
        }
        auto now=std::chrono::steady_clock::now();
        files->tick(serial,session,online,page==7);
        if(online && page==3 && !infoTask && !deviceCommandBusy() && now>=nextInfo)refreshDevice();
        auto video=videoMirror->state();if(video.active&&!video.stopping&&(!online||video.serial!=serial))mirrorUi.push_back(videoMirror->stop());
    }
    void reloadPlugins(){
        if(!pluginRuns.empty()){cyui::notify("Attendez la fin de la commande de plugin avant de recharger.",cyui::Tone::warning);return;}
        plugins=loadPlugins({appRoot()/L"plugins",userRoot()/L"plugins"});if(selectedPlugin>=int(plugins.size()))selectedPlugin=0;
        for(const auto& p:plugins)if(configurablePlugin(p)&&(!pluginPanels.count(p.id)||pluginPanels[p.id].controller!=p.controller)){
            auto& panel=pluginPanels[p.id];panel=PluginPanel{};panel.controller=p.controller;panel.draft=pluginDefaults(p);
            if(!options.demo){try{auto path=userRoot()/L"plugin-settings"/(widen(p.id)+L".json");std::error_code ec;if(std::filesystem::is_regular_file(path,ec)&&std::filesystem::file_size(path,ec)<16384){std::ifstream input(path);Json j;input>>j;panel.draft=validatePluginSettings(p,j);}}catch(const std::exception& e){panel.error=std::string("Profil local ignoré : ")+e.what();}}
        }
    }
    void savePluginProfile(const Plugin& p){
        if(options.demo)return;try{auto values=validatePluginSettings(p,pluginPanels[p.id].draft);auto folder=userRoot()/L"plugin-settings";std::filesystem::create_directories(folder);auto path=folder/(widen(p.id)+L".json"),temporary=folder/(widen(p.id)+L".tmp");std::ofstream output(temporary,std::ios::binary);output<<values.dump(2);output.close();if(!output||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Écriture du profil impossible.");cyui::notify("Profil local enregistré.",cyui::Tone::success);}catch(const std::exception& e){pluginPanels[p.id].error=e.what();}
    }
    bool pluginBusy()const{return !pluginRuns.empty();}
    uint64_t runPlugin(const Plugin& p,const std::string& action,const Json& settings){
        if(pluginBusy())throw std::runtime_error("Attendez la fin de la commande de plugin en cours.");
        auto job=metricsPluginJob(p,action,settings);auto number=submit(job.label,job.steps,"plugin:control",{},job.timeoutMs,job.cleanupSteps);pluginRuns[number]={p,action,settings};auto& panel=pluginPanels[p.id];panel.task=number;panel.error.clear();panel.message="Commande en cours…";return number;
    }
    uint64_t submit(const std::string& label,std::vector<CommandStep> steps,const std::string& kind="command",const std::wstring& path={},int timeout=30000,std::vector<CommandStep> cleanup={}) {
        if(!backend)return 0;JobRequest request;request.label=label;request.steps=std::move(steps);request.timeoutMs=timeout;request.cleanupSteps=std::move(cleanup);auto id=backend->submit(std::move(request));
        tasks[id]={id,label,"",kind,false,false,0,path};taskOrder.push_back(id);if(kind!="info"){selectedTask=id;}
        while(taskOrder.size()>100){auto at=std::find_if(taskOrder.begin(),taskOrder.end(),[&](uint64_t n){return tasks[n].done;});if(at==taskOrder.end())break;tasks.erase(*at);taskOrder.erase(at);}return id;
    }
    void jobEvent(const BackendEvent& e) {
        auto found=tasks.find(e.job);if(found==tasks.end())return;auto& task=found->second;
        if(e.kind==BackendEvent::JobOutput){files->event(e);if(task.output.size()<2*1024*1024)task.output+=e.text;return;}
        if(e.kind!=BackendEvent::JobDone)return;task.done=true;task.ok=e.ok;task.code=e.code;task.output+="\n"+e.text+"\n";
        if(auto completion=files->event(e)){task.ok=completion->ok;task.output+="\n"+completion->message;return;}
        if(task.kind=="files:query"&&e.ok){try{auto listing=readListing(task.path);auto& result=nativeOutputs[e.job];Json rows=Json::array();auto offset=result.value("offset",0),limit=result.value("limit",100);size_t bytes=0;for(size_t i=size_t(offset);i<listing.entries.size()&&rows.size()<size_t(limit)&&bytes<250000;++i){const auto& file=listing.entries[i];Json row={{"name",file.name},{"path",remotePath(result.at("path").get<std::string>()+"/"+file.name)},{"type",file.type=='d'?"directory":file.type=='f'?"file":file.type=='l'?"symlink":"other"},{"size",file.size?Json(*file.size):Json(nullptr)},{"modified",file.modified?Json(*file.modified):Json(nullptr)},{"target",file.target}};bytes+=row.dump(-1,' ',false,Json::error_handler_t::replace).size();rows.push_back(std::move(row));}result["nextOffset"]=size_t(offset)+rows.size()<listing.entries.size()?Json(size_t(offset)+rows.size()):Json(nullptr);result["entries"]=rows;result["count"]=listing.entries.size();result["truncated"]=listing.truncated;}catch(const std::exception& error){task.ok=false;task.output+="\n"+std::string(error.what());}}
        if(task.kind=="device:control")nextInfo=std::chrono::steady_clock::now();
        if(task.kind=="info") {
            infoTask=0;nextInfo=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            std::istringstream lines(task.output);std::string line;while(std::getline(lines,line)){auto at=line.find('=');if(at!=std::string::npos && line.rfind("QD_",0)==0)deviceInfo[line.substr(3,at-3)]=trim(line.substr(at+1));}return;
        }
        if(task.kind.rfind("link:",0)==0)pluginLinked[task.kind.substr(5)]=e.ok;
        if(task.kind.rfind("unlink:",0)==0 && e.ok)pluginLinked[task.kind.substr(7)]=false;
        if(task.kind=="record"){recordTask=0;}
        if(pluginRuns.count(e.job)){
            auto run=pluginRuns[e.job];auto& panel=pluginPanels[run.plugin.id];
            if(e.session!=session){task.ok=false;task.output+="\nLe casque sélectionné a changé.";}
            else{
                if(task.ok){try{
                    Json capture;auto result=metricsPluginResult(run.plugin,run.action,task.output,run.settings,&capture);
                    if(capture.is_object()&&!capture.empty()){
                        panel.capture=capture;
                        if(pluginCapturePaths.count(e.job)){auto path=pluginCapturePaths[e.job];std::ofstream output(path,std::ios::binary);output<<capture.dump();output.close();if(!output)throw std::runtime_error("Écriture de la capture privée impossible.");result["file"]=narrow(path);result["mimeType"]="application/json";}
                    }
                    if(nativeJobs.count(e.job))nativeOutputs[e.job]=result;panel.observed=result;panel.observed.erase("file");panel.observed.erase("mimeType");
                    if(run.plugin.controller=="ovr_metrics"&&result.contains("state")){const auto& state=result["state"];panel.available.clear();for(const auto& m:hudMetrics()){bool metricFound=false;for(const auto& item:state.at("metrics")){auto name=item.value("name",std::string());metricFound|=name==m.title||name.rfind(std::string(m.title)+" (",0)==0;}panel.available[m.id]=metricFound;}}
                    if(run.action=="catalog"&&result.contains("metrics")){panel.available.clear();for(const auto& item:result["metrics"])panel.available[item.at("id").get<std::string>()]=item.value("available",false);}
                    if(run.action=="capture")panel.requestedCapture=result.at("capture_id").get<std::string>();
                    panel.message=run.action=="capture"?"Capture demandée. Actualisez l’état et vérifiez son capture_id.":run.action=="apply"?"Réglages du HUD vérifiés par le service.":"Réponse reçue du service.";
                    if(run.action=="read_state"&&!panel.requestedCapture.empty()){if(panel.capture.value("capture_id",std::string())==panel.requestedCapture&&!result.value("capturing",false))panel.message="Capture reçue · "+panel.capture.value("status",std::string("état inconnu"))+" · "+panel.requestedCapture;else panel.message="En attente de la capture "+panel.requestedCapture+". Actualisez l’état.";}
                    panel.error.clear();
                }catch(const std::exception& error){task.ok=false;task.output+="\n"+std::string(error.what());}}
                if(!task.ok){panel.error=task.output.substr(task.output.size()>2000?task.output.size()-2000:0);panel.message.clear();}panel.task=0;
            }
            pluginRuns.erase(e.job);pluginCapturePaths.erase(e.job);
        }
        if(task.ok&&nativeOutputs.count(e.job)&&nativeOutputs[e.job].contains("file")){auto output=widen(nativeOutputs[e.job]["file"].get<std::string>());std::error_code ec;bool directory=nativeOutputs[e.job].value("mimeType",std::string())=="inode/directory";bool exists=directory?std::filesystem::is_directory(output,ec):std::filesystem::is_regular_file(output,ec);if(!exists||ec){task.ok=false;task.output+="\nLe fichier de sortie annoncé n’a pas été créé.";}}
        if(nativeJobs.count(e.job)) {auto id=nativeJobs[e.job];Json result={{"exitCode",e.code}};if(task.kind!="plugin:control")result["output"]=task.output.substr(0,100000);if(nativeOutputs.count(e.job))result.update(nativeOutputs[e.job]);auto error=task.ok?"":task.output.substr(task.kind=="plugin:control"&&task.output.size()>4000?task.output.size()-4000:0,4000);assistant->send({{"type","native_result"},{"id",id},{"ok",task.ok},{"result",result},{"error",error}});nativeJobs.erase(e.job);nativeOutputs.erase(e.job);}
        if(task.kind=="screenshot" && e.ok){std::string error;preview->load(task.path,error);}
        cyui::notify((task.label+(task.ok?" : terminé":" : échec (voir ADB)")).c_str(),task.ok?cyui::Tone::success:cyui::Tone::error);
    }
    Json contextSnapshot() {
        Json context={{"application","CyVR QuestDev"},{"version",CoreVersion},{"session",session},{"selectedDevice",serial},{"online",online},{"capturing",capturing},{"page",page},{"selectedPackage",view.package},{"devices",Json::array()},{"applications",Json::array()},{"plugins",Json::array()},{"recentLogs",Json::array()},{"deviceInfo",deviceInfo}};
        context["pages"]={{"console",0},{"applications",1},{"settings",2},{"device",3},{"adb",4},{"plugins",5},{"assistant",6},{"files",7},{"metrics",8}};
        context["metrics"]=metrics->describe();
        context["updates"]={{"current",AppVersion},{"phase",updateState.value("phase",std::string("idle"))},{"available",updateState.contains("release")&&!updateState["release"].is_null()?updateState["release"].value("version",std::string()):std::string()},{"usage","Settings > Mises à jour GitHub. Authentication, download and installation are native UI actions."}};
        context["mirroring"]=videoMirror->describe();
        context["mirroringUsage"]="Device > Mirroring vidéo / audio starts real H.264 video in a native window. F11 toggles fullscreen. mirror_start/stop device actions await first texture / graceful shutdown. Optional mirror settings: maxSize, maxFps, bitRateMbps, audio=off|duplicate|output, fullscreen. Audio duplication keeps headset playback on Android 13+, but apps may opt out. Native UI can record MKV during the session. Stop on device change/disconnect/app shutdown. Does not start apps, change Wi-Fi mode or kill the shared ADB server.";
        context["metricsUsage"]="Metrics page: open OVR CSV, select columns, shared time zoom/pan/cursor. Refresh reports with metrics_action to discover remote paths; read_metrics returns bounded samples (missing=null), seconds from first CSV timestamp. record_metrics waits for CSV collection/restoration, needs a running VR app, and may return filesSaved=0. Follow cadence depends on OVR file writes. Export Core recordings before FinishTask. UI recordings persist on PC.";
        context["fileBrowser"]=files->describe();
        context["logRetention"]={{"maxLines",store.capacity()},{"maxBytes",store.byteLimit()},{"lines",store.entries().size()},{"bytes",store.bytes()},{"discarded",store.discarded()}};
        context["consoleFilters"]={{"search",view.search.data()},{"exclude",view.exclude.data()},{"tag",view.tag.data()},{"minimumIndex",view.minimum},{"pid",view.pid},{"paused",view.paused},{"follow",view.follow},{"showProcessEvents",view.events}};
        context["usage"]={"Files: optional PC/Quest side-by-side browser, Ctrl/Shift multi-selection. Prepare a list to hold transfers, then start/cancel the sequential batch; otherwise transfers start immediately. Queued destinations are bound to their Quest; device changes pause the batch. Browse accessible Quest folders, transfer regular files, create/rename entries and delete files or empty directories. No root access or recursive deletion. Core imports upload sources automatically and owns downloaded task files.","Each page has its own sidebar. Select a package in Console to open its retained application log.","Logs are assigned by timestamped PID lifetime and exact package/subprocess name; unknown origins remain system/unassigned. Shared UIDs do not imply shared applications.","Settings > Limites du journal configures retained lines and memory (logRetention). Pausing a view does not stop capture.","Device Actions uses asynchronous ADB jobs; ADB page shows output, history and cancellation. Cast uses a managed scrcpy H.264 video/audio window.","Plugins are validated .cyplugin manifests; an ADB tunnel requires an actual TCP service in the target app.","Core operation files belong to private tasks. Export results before FinishTask deletes temporary files.","Native QuestDev tool calls are available in Codex. Other integrated providers currently receive text context; external AI clients can use the MCP catalogue."};
        for(const auto& d:devices)context["devices"].push_back({{"serial",d.serial},{"state",d.state},{"model",d.model}});
        for(const auto& p:packages){auto c=counts[p.name];context["applications"].push_back({{"package",p.name},{"system",p.system},{"processCount",c.processes},{"logCount",c.lines},{"errors",c.errors}});}
        for(const auto& p:plugins){Json actions=pluginControlActions(p);for(const auto& a:p.actions)actions.push_back({{"id",a.id},{"label",a.label}});Json record={{"id",p.id},{"name",p.name},{"package",p.package},{"enabled",p.enabled},{"linked",pluginLinked[p.id]},{"actions",actions},{"controller",p.controller},{"installed",std::any_of(packages.begin(),packages.end(),[&](const Package& pkg){return pkg.name==p.package;})}};if(configurablePlugin(p)){auto& panel=pluginPanels[p.id];record["settingsSchema"]=pluginSettingsSchema(p);record["defaults"]=pluginDefaults(p);record["draft"]=panel.draft;record["observed"]=panel.observed;record["availableMetrics"]=panel.available;record["busy"]=panel.task!=0;record["error"]=panel.error.substr(0,1000);record["captureDirectory"]=p.controller=="ovr_metrics"?"/sdcard/Android/data/com.oculus.ovrmonitormetricsservice/files/CapturedMetrics":"/sdcard/Android/data/org.cyinsights.ovr/files/Captures";}context["plugins"].push_back(record);}
        if(sendLogs){updateView(view);size_t start=view.ids.size()>50?view.ids.size()-50:0;for(size_t i=start;i<view.ids.size();++i)if(auto* e=find(view.ids[i]))context["recentLogs"].push_back({{"epoch",e->epoch},{"package",e->package},{"pid",e->pid},{"level",std::string(1,e->level)},{"tag",e->tag},{"message",e->message.substr(0,1000)}});}
        context["permissions"]=aiPolicy==0?"read-only":aiPolicy==1?"ask-before-actions":"allow-actions";
        return context;
    }
    void startAssistant() {
        if(options.demo)return;std::string error;auto root=productRoot();
        if(assistant->start((root/L"runtime/python/Scripts/python.exe").wstring(),(root/L"assistant/service.py").wstring(),(userRoot()/L"assistant").wstring(),error)){assistantStatus="Démarrage du runtime CyToolsCore…";assistantReady=false;assistantConnected=false;}
        else assistantStatus=error;
    }
    void replyNative(const std::string& id,bool ok,const Json& result=Json::object(),const std::string& error=""){assistant->send({{"type","native_result"},{"id",id},{"ok",ok},{"result",result},{"error",error}});}
    void tickAssistant() {
        if(!assistant)return;
        for(auto& e:assistant->poll()){
            auto type=e.value("type",std::string());
            if(type=="ready"){assistantReady=true;assistantStatus="CyToolsCore "+e.value("sdkVersion",std::string())+" · MCP disponible";mcpConfig=e.value("mcpConfig",std::string());connectionFile=e.value("connectionFile",std::string());}
            else if(type=="connected"){assistantConnected=true;assistantStatus=e.value("connector",std::string())+" connecté";aiModels=e.value("models",Json::array());if(aiModel[0]==0)strncpy_s(aiModel.data(),aiModel.size(),e.value("model",std::string()).c_str(),_TRUNCATE);}
            else if(type=="idle")assistantBusy=false;
            else if(type=="new_chat")chat.clear();
            else if(type=="error"){assistantStatus=e.value("message",std::string());assistantBusy=false;chat.push_back({"Erreur",assistantStatus});}
            else if(type=="provider_event"){
                auto record=e.value("event",Json::object());auto kind=record.value("type",std::string());
                if(kind=="TextDelta"){if(chat.empty()||chat.back().first!="Assistant")chat.push_back({"Assistant",""});chat.back().second+=record.value("text",std::string());}
            }else if(type=="chat_done"){auto answer=e.value("text",std::string());auto lastUser=std::find_if(chat.rbegin(),chat.rend(),[](const auto& line){return line.first=="Vous";});auto start=lastUser==chat.rend()?chat.begin():lastUser.base();chat.erase(std::remove_if(start,chat.end(),[](const auto& line){return line.first=="Assistant";}),chat.end());chat.push_back({"Assistant",answer});}
            else if(type=="assistant_tool")chat.push_back({"Outil",e.value("operation",std::string())+" · "+e.value("status",std::string())});
            else if(type=="native_call"){
                if(e.value("deviceSession",uint64_t(0))!=session){replyNative(e.value("id",std::string()),false,Json::object(),"Le casque sélectionné a changé.");continue;}
                auto op=e.value("operation",std::string());auto parameters=e.value("parameters",Json::object());auto action=parameters.value("action",std::string());
                bool pluginRead=op=="plugin_action"&&(action=="read_state"||action=="catalog"||action=="status")&&std::any_of(plugins.begin(),plugins.end(),[&](const Plugin& p){return configurablePlugin(p)&&p.id==parameters.value("plugin",std::string());});
                bool mutating=op=="adb_execute"||op=="device_action"||op=="application_action"||(op=="plugin_action"&&!pluginRead)||op=="push_file"||op=="file_action"||op=="record_metrics"||(op=="metrics_action"&&action=="stop_record");
                if(mutating&&aiPolicy==0)replyNative(e.value("id",std::string()),false,Json::object(),"Actions refusées : lecture seule.");
                else if(mutating&&aiPolicy==1)approvals.push_back(e);else executeNative(e);
            }else if(type=="native_cancel"){
                auto id=e.value("id",std::string());auto at=std::find_if(approvals.begin(),approvals.end(),[&](const Json& a){return a.value("id",std::string())==id;});if(at!=approvals.end()){approvals.erase(at);replyNative(id,false,Json::object(),"Annulé.");}
                for(const auto& [job,request]:nativeJobs)if(request==id&&backend)backend->cancel(job);
                if(nativeMetrics.count(id))metrics->capture().cancel(nativeMetrics[id].first.id);
                if(nativeMirrors.count(id))videoMirror->cancel(nativeMirrors[id].first.id);
            }
        }
        auto now=std::chrono::steady_clock::now();if(assistantReady&&assistant->running()&&now>=nextContext){assistant->send({{"type","context"},{"context",contextSnapshot()}});nextContext=now+std::chrono::seconds(1);}
        if(assistantReady&&!assistant->running()){assistantReady=assistantConnected=assistantBusy=false;assistantStatus="Runtime arrêté. Les détails sont dans runtime-errors.log.";}
    }
    void executeNative(const Json& request) {
        auto id=request.value("id",std::string()),op=request.value("operation",std::string());auto p=request.value("parameters",Json::object());
        if(request.value("deviceSession",uint64_t(0))!=session){replyNative(id,false,Json::object(),"Le casque a changé.");return;}
        try{
            if(op=="device_action"&&(p.value("action",std::string())=="mirror_start"||p.value("action",std::string())=="mirror_stop")){
                bool start=p.at("action")=="mirror_start";if(start&&(!online||options.demo))throw std::runtime_error("Aucun Quest connecté.");
                MirrorTicket ticket;if(start){auto values=p.value("mirror",Json::object());MirrorOptions config;config.maxSize=values.value("maxSize",1920);config.maxFps=values.value("maxFps",60);config.bitRateMbps=values.value("bitRateMbps",12);config.fullscreen=values.value("fullscreen",false);auto audio=values.value("audio",std::string("duplicate"));config.audio=audio=="off"?0:audio=="duplicate"?1:audio=="output"?2:-1;ticket=videoMirror->start(mirrorEngine,adb,serial,config);}else ticket=videoMirror->stop();
                nativeMirrors[id]={std::move(ticket),session};return;
            }
            if(op=="read_metrics"){replyNative(id,true,metrics->capture().query(p));return;}
            if(op=="metrics_action"||op=="record_metrics"){
                metrics->capture().device(adb,serial,session,online&&!options.demo);
                std::string action=op=="record_metrics"?"record_timed":p.at("action").get<std::string>();std::filesystem::path path;
                if(action=="open_csv")path=widen(p.at("sourceFile").get<std::string>());else if(action=="export_csv"||action=="record_timed")path=widen(p.at("_output").get<std::string>());
                auto ticket=metrics->capture().request(action,path,p.value("path",std::string()),p.value("seconds",10));nativeMetrics[id]={std::move(ticket),session};return;
            }
            if(op=="list_applications"){Json apps=Json::array();auto query=lower(p.value("query",std::string())),category=p.value("category",std::string("all"));for(const auto& pkg:packages){auto c=counts[pkg.name];if(lower(pkg.name).find(query)==std::string::npos||(category=="user"&&pkg.system)||(category=="system"&&!pkg.system)||(category=="running"&&!c.processes))continue;apps.push_back({{"package",pkg.name},{"uid",pkg.uid},{"system",pkg.system},{"processes",c.processes},{"logs",c.lines}});}replyNative(id,true,{{"applications",apps}});return;}
            if(op=="list_plugins"){replyNative(id,true,{{"plugins",contextSnapshot()["plugins"]}});return;}
            if(op=="navigate"){const std::map<std::string,int> pages={{"console",0},{"applications",1},{"settings",2},{"device",3},{"adb",4},{"plugins",5},{"assistant",6},{"files",7},{"metrics",8}};page=pages.at(p.value("page",std::string()));auto pkg=p.value("package",std::string());if(!pkg.empty())openApp(pkg);replyNative(id,true,{{"page",page}});return;}
            if(op=="read_logs"||op=="export_logs"){
                LogFilter filter;auto pkg=p.value("package",std::string());if(!pkg.empty())filter.packages.insert(pkg);filter.text=lower(p.value("text",std::string()));filter.minimum=severity(p.value("minimum",std::string("V"))[0]);
                std::vector<const LogEntry*> entries;for(const auto& e:store.entries())if(filter.matches(e))entries.push_back(&e);
                if(op=="export_logs"){auto path=widen(p.at("_output").get<std::string>());std::ofstream out(path,std::ios::binary);out<<exportLogs(entries,p.value("format",std::string())=="jsonl");out.close();if(!out)throw std::runtime_error("Échec de l’export.");replyNative(id,true,{{"file",narrow(path)},{"mimeType",p.value("format",std::string())=="jsonl"?"application/x-ndjson":"text/plain"},{"count",entries.size()}});}
                else {int limit=std::clamp(p.value("limit",100),1,2000);Json rows=Json::array();size_t start=entries.size()>size_t(limit)?entries.size()-size_t(limit):0;size_t bytes=0;for(size_t i=start;i<entries.size()&&bytes<200000;++i){auto* e=entries[i];auto message=e->message.substr(0,8000);bytes+=message.size()+e->tag.size()+e->package.size()+150;rows.push_back({{"epoch",e->epoch},{"package",e->package},{"process",e->process},{"pid",e->pid},{"tid",e->tid},{"level",std::string(1,e->level)},{"tag",e->tag},{"message",message}});}replyNative(id,true,{{"logs",rows},{"matched",entries.size()}});}return;
            }
            if(!online||!backend)throw std::runtime_error("Aucun casque connecté.");
            JobRequest job;job.label="IA · "+op;Json result=Json::object();std::string taskKind="ai";std::wstring taskPath;
            if(op=="list_files"){auto path=remotePath(p.value("path",std::string("/sdcard")));taskPath=widen(p.at("_output").get<std::string>());job=listRemoteFiles(path,taskPath);taskKind="files:query";result={{"path",path},{"offset",p.value("offset",0)},{"limit",p.value("limit",100)}};
            }else if(op=="pull_file"){auto output=p.at("_output").get<std::string>();job=pullRemoteFile(p.at("path").get<std::string>(),widen(output));result={{"file",output},{"mimeType","application/octet-stream"},{"source",remotePath(p.at("path").get<std::string>())}};
            }else if(op=="push_file"){job=pushRemoteFile(widen(p.at("sourceFile").get<std::string>()),p.at("destination").get<std::string>(),p.value("overwrite",false));taskKind="files:change";result={{"destination",remotePath(p.at("destination").get<std::string>())}};
            }else if(op=="file_action"){job=changeRemoteFile(p.at("action").get<std::string>(),p.at("path").get<std::string>(),p.value("name",std::string()));taskKind="files:change";
            }else if(op=="adb_execute"){
                auto text=p.at("command").get<std::string>();std::vector<std::string> args;std::string error;
                if(p.value("mode",std::string("shell"))=="shell")args={"shell",text};else{args=parseCommand(text,error);if(!args.empty()&&(args[0]=="adb"||args[0]=="adb.exe"))args.erase(args.begin());if(args.empty()||args[0].empty()||args[0][0]=='-')throw std::runtime_error("Commande ADB sans option globale attendue.");}
                if(!error.empty())throw std::runtime_error(error);job.steps={{args,{}}};job.timeoutMs=std::clamp(p.value("timeout",30),1,300)*1000;
            }else if(op=="application_action"){
                auto package=p.at("package").get<std::string>();if(!validPackage(package))throw std::runtime_error("Package invalide.");
                std::string script=p.value("action",std::string())=="stop"?"am force-stop "+shellQuote(package):"c=$(cmd package resolve-activity --brief -a android.intent.action.MAIN -c android.intent.category.LAUNCHER "+shellQuote(package)+" | tail -1); case \"$c\" in */*) am start -n \"$c\";; *) echo 'No launch activity'; exit 1;; esac";job.steps={{{"shell",script},{}}};
            }else if(op=="plugin_action"){
                auto it=std::find_if(plugins.begin(),plugins.end(),[&](const Plugin& plugin){return plugin.id==p.value("plugin",std::string());});if(it==plugins.end())throw std::runtime_error("Plugin introuvable.");auto action=p.value("action",std::string());
                if(!it->enabled)throw std::runtime_error("Plugin désactivé.");
                if(configurablePlugin(*it)){auto number=runPlugin(*it,action,p.value("settings",Json::object()));nativeJobs[number]=id;if(p.contains("_output"))pluginCapturePaths[number]=widen(p.at("_output").get<std::string>());return;}
                if(!p.value("settings",Json::object()).empty())throw std::runtime_error("Cette action de plugin ne prend pas de réglages.");
                if(action=="connect"||action=="disconnect"){job=pluginLink(*it,action=="disconnect");taskKind=(action=="connect"?"link:":"unlink:")+it->id;}else{auto a=std::find_if(it->actions.begin(),it->actions.end(),[&](const PluginAction& a){return a.id==action;});if(a==it->actions.end())throw std::runtime_error("Action de plugin introuvable.");job=pluginAction(*it,*a);}
            }else if(op=="device_action"){
                auto action=p.at("action").get<std::string>();const std::map<std::string,std::string> scripts={{"wake","input keyevent 224"},{"sleep","input keyevent 223"},{"gpu_on","setprop debug.vr.gpuprofilingservice 1"},{"gpu_off","setprop debug.vr.gpuprofilingservice 0"},{"proximity_on","am broadcast --user 0 -a com.oculus.vrpowermanager.automation_disable"},{"proximity_off","am broadcast --user 0 -a com.oculus.vrpowermanager.prox_close --ei duration 3600000"},{"boundary_on","setprop debug.oculus.guardian_pause 0; am broadcast -a com.oculus.intent.action.GUARDIAN_STATE_CHANGED --ez guardian_enabled true"},{"boundary_off","setprop debug.oculus.guardian_pause 1; am broadcast -a com.oculus.intent.action.GUARDIAN_STATE_CHANGED --ez guardian_enabled false"},{"hud_on",metricsCommand("ENABLE_STATS")+"; "+metricsCommand("ENABLE_OVERLAY")},{"hud_off",metricsCommand("DISABLE_OVERLAY")}};
                if(action=="hud_on")job.steps={{{"shell",metricsCommand("ENABLE_STATS")+"; "+metricsCommand("ENABLE_OVERLAY")},{}}};
                else if(action=="hud_off")job.steps={{{"shell",metricsCommand("DISABLE_OVERLAY")},{}}};
                else if(action=="metrics_on"||action=="metrics_off")job.steps={{{"shell",metricsCommand(action=="metrics_on"?"ENABLE_CSV":"DISABLE_CSV")},{}}};
                else if(action=="metrics_export"){auto output=p.at("_output").get<std::string>();job.steps={{{"pull","/sdcard/Android/data/com.oculus.ovrmonitormetricsservice/files/CapturedMetrics",output},{}}};result={{"file",output},{"mimeType","inode/directory"}};}
                else if(action=="record_video"){if(recordTask)throw std::runtime_error("Un enregistrement est déjà en cours.");job=videoJob(widen(p.at("_output").get<std::string>()),p.value("seconds",10));taskKind="record";result={{"file",p.at("_output")},{"mimeType","video/mp4"}};}
                else if(action=="video_stop"){if(!recordTask)throw std::runtime_error("Aucun enregistrement à finaliser.");job.steps={{{"shell",finishVideoCommand()},{}}};}
                else if(scripts.count(action))job.steps={{{"shell",scripts.at(action)},{}}};
                else if(action=="reboot")job.steps={{{"reboot"},{}}};else if(action=="wifi_on")job.steps={{{"tcpip","5555"},{}}};else if(action=="usb")job.steps={{{"usb"},{}}};
                else if(action=="browser"){auto url=p.value("url",std::string());if(url.rfind("https://",0)!=0&&url.rfind("http://",0)!=0)throw std::runtime_error("URL HTTP/HTTPS requise.");job.steps={{{"shell","am start -a android.intent.action.VIEW -d "+shellQuote(url)},{}}};}
                else if(action=="screenshot"){auto output=p.at("_output").get<std::string>();job.steps={{{"exec-out","screencap","-p"},widen(output)}};result={{"file",output},{"mimeType","image/png"}};}
                else if(action=="perfetto"){auto output=p.at("_output").get<std::string>();auto remote="/data/misc/perfetto-traces/questdev-"+std::to_string(GetTickCount64())+".perfetto-trace";auto seconds=std::clamp(p.value("seconds",10),1,120);job.timeoutMs=(seconds+30)*1000;job.steps={{{"shell","perfetto","-o",remote,"-t",std::to_string(seconds)+"s","-b","32mb","sched","freq","idle","am","wm","gfx","view","binder_driver"},{}},{{"pull",remote,output},{}},{{"shell","rm -f "+remote},{}}};result={{"file",output},{"mimeType","application/octet-stream"}};}
                else throw std::runtime_error("Action appareil non prise en charge.");
            }else throw std::runtime_error("Opération inconnue.");
            auto number=submit(job.label,job.steps,taskKind,taskPath,job.timeoutMs,job.cleanupSteps);if(taskKind=="record")recordTask=number;nativeJobs[number]=id;nativeOutputs[number]=result;
        }catch(const std::exception& e){replyNative(id,false,Json::object(),e.what());}
    }
    bool deviceCommandBusy()const{for(const auto& [id,task]:tasks)if(task.kind=="device:control"&&!task.done)return true;return false;}
    void refreshDevice(bool readMetrics=false) {
        const std::string script="echo QD_MODEL=$(getprop ro.product.model); echo QD_OS=$(getprop ro.build.version.release); echo QD_BUILD=$(getprop ro.build.display.id); echo QD_BATTERY=$(dumpsys battery | sed -n 's/.*level: //p'); echo QD_GPU=$(getprop debug.vr.gpuprofilingservice); echo QD_GUARDIAN=$(getprop debug.oculus.guardian_pause); echo QD_WIFI=$(getprop service.adb.tcp.port); echo QD_IP=$(ip -o -4 addr show wlan0 | awk '{print $4}'); echo QD_STORAGE=$(df -h /sdcard | tail -1); echo QD_PROX=$(dumpsys oculus.internal.power.IVrPowerManager/default | grep 'Virtual proximity state'); echo QD_METRICS=$(pm path com.oculus.ovrmonitormetricsservice | head -1)";
        infoTask=submit("État du casque",{{{"shell",script},{}}},"info");
        if(!pluginBusy())for(const auto& p:plugins)if(p.controller=="ovr_metrics"&&std::any_of(packages.begin(),packages.end(),[&](const Package& app){return app.name==p.package;})){
            auto& panel=pluginPanels[p.id];if(readMetrics||(panel.observed.empty()&&panel.error.empty()))try{runPlugin(p,"read_state",Json::object());}catch(const std::exception& e){panel.error=e.what();}break;
        }
    }
    void shellAction(const std::string& label,const std::string& script,bool confirm=false) {
        JobRequest request;request.label=label;request.steps={{{"shell",script},{}}};
        if(confirm){confirmationText=label;confirmationJob=std::move(request);confirmDevice=true;}
        else submit(label,request.steps,"device:control");
        if(script.find("ovrmonitormetricsservice")!=std::string::npos)for(auto& [id,panel]:pluginPanels)if(panel.controller=="ovr_metrics"){panel.observed=Json::object();panel.error.clear();}
        nextInfo=std::chrono::steady_clock::now();
    }
    JobRequest videoJob(const std::wstring& path,int seconds) {
        seconds=std::clamp(seconds,1,180);auto suffix=std::to_string(GetTickCount64());
        recordRemote="/sdcard/questdev-"+suffix+".mp4";recordPid="/data/local/tmp/questdev-"+suffix+".pid";
        // A successful SIGINT finalizes the file even if wait returns 130.
        auto script="screenrecord --time-limit "+std::to_string(seconds)+" "+recordRemote+" & p=$!; echo $p > "+recordPid+"; wait $p; test -s "+recordRemote;
        JobRequest job{"Enregistrement vidéo",{{{"shell",script},{}},{{"pull",recordRemote,narrow(path)},{}},{{"shell","rm -f "+recordPid+" "+recordRemote},{}}},(seconds+30)*1000,true};
        auto cleanup="p=$(cat "+recordPid+" 2>/dev/null); if test -n \"$p\" && grep -q "+shellQuote(recordRemote)+" /proc/$p/cmdline; then kill -2 $p; i=0; while kill -0 $p 2>/dev/null && test $i -lt 20; do sleep 0.1; i=$((i+1)); done; grep -q "+shellQuote(recordRemote)+" /proc/$p/cmdline 2>/dev/null && kill -9 $p; fi; rm -f "+recordPid+" "+recordRemote;
        job.cleanupSteps={{{"shell",cleanup},{}}};return job;
    }
    std::string finishVideoCommand() const {return "p=$(cat "+recordPid+" 2>/dev/null); test -n \"$p\" && grep -q "+shellQuote(recordRemote)+" /proc/$p/cmdline && kill -2 $p";}
    void executeTerminal() {
        auto text=trim(command.data());if(text.empty()||!backend||!online||options.demo)return;std::vector<std::string> args;std::string error;
        if(terminalMode==0)args={"shell",text};else {args=parseCommand(text,error);if(!args.empty()&&(args[0]=="adb"||args[0]=="adb.exe"))args.erase(args.begin());if(!args.empty()&&(args[0].empty()||args[0].front()=='-'))error="La cible est le casque sélectionné. Entrez une commande sans option globale ADB.";}
        if(!error.empty()||args.empty()){cyui::notify(error.empty()?"Commande vide.":error.c_str(),cyui::Tone::error);return;}
        submit(text,{{args,{}}},"terminal",{},std::clamp(terminalTimeout,1,300)*1000);
    }
    void rebuildCounts() {
        if(countRevision==store.revision())return;counts.clear();
        for(const auto& e:store.entries()) { auto& c=counts[e.package];++c.lines;if(severity(e.level)>=4)++c.errors;if(e.level=='W')++c.warnings; }
        for(const auto& p:store.processes())++counts[store.packageFor(p.name)].processes;
        countRevision=store.revision();
    }
    void openApp(const std::string& p) {
        if(!p.empty() && p!="@system" && std::find(tabs.begin(),tabs.end(),p)==tabs.end())tabs.push_back(p);
        view.package=p;view.dirty=true;view.selected=0;tabRequest=true;
    }
    const LogEntry* find(uint64_t id) const {
        if(store.entries().empty())return nullptr;
        auto first=store.entries().front().id;if(id<first)return nullptr;size_t index=size_t(id-first);
        return index<store.entries().size() && store.entries()[index].id==id ? &store.entries()[index] : nullptr;
    }
    void updateView(View& v) {
        if(!v.dirty && v.revision==store.revision())return;
        if(v.dirty)v.rawInvalidate=true;
        LogFilter filter;filter.text=lower(v.search.data());filter.exclude=lower(v.exclude.data());filter.tag=lower(v.tag.data());filter.minimum=v.minimum;filter.pid=v.pid;filter.hideLifecycle=!v.events;
        filter.onlyUnassigned=v.package=="@system";if(!v.package.empty() && !filter.onlyUnassigned)filter.packages.insert(v.package);
        v.ids.clear();for(const auto& e:store.entries())if((!v.paused || e.id<=v.freeze) && filter.matches(e))v.ids.push_back(e.id);
        if(!v.rawMode)std::sort(v.ids.begin(),v.ids.end(),[&](auto a,auto b){return logLess(*find(a),*find(b),v.sort);});
        v.dirty=false;v.revision=store.revision();v.rawRevision=~uint64_t(0);
    }
    std::vector<const LogEntry*> visible(View& v) { updateView(v);std::vector<const LogEntry*> out;for(auto id:v.ids)if(auto* e=find(id))out.push_back(e);return out; }
    std::string viewText(View& v,bool jsonl=false){auto entries=visible(v);if(jsonl||!v.rawMode)return exportLogs(entries,jsonl);std::string text;for(auto* e:entries){text+=rawLog(*e);text+='\n';}return text;}
    void exportView(View& v,bool jsonl) {
        auto path=fileDialog(true,jsonl?L"JSON Lines (*.jsonl)\0*.jsonl\0\0":L"Journal texte (*.log)\0*.log\0\0",jsonl?L"jsonl":L"log",jsonl?L"questdev.jsonl":L"questdev.log");
        if(path.empty())return;std::ofstream out(path,std::ios::binary);out<<viewText(v,jsonl);out.close();
        cyui::notify(out?"Journal exporté":"Échec de l’export",out?cyui::Tone::success:cyui::Tone::error);
    }
    void pause(View& v) { v.paused=!v.paused;if(v.paused)v.freeze=store.entries().empty()?0:store.entries().back().id;v.dirty=true; }
    void install() {
        auto p=fileDialog(false,L"Application Android (*.apk)\0*.apk\0\0",L"apk");
        if(!p.empty()){backend->action("install",narrow(p));status="Installation de l’APK…";}
    }
    void topbar() {
        if(cyui::beginTopBar("##top",cyui::dp(62))) {
            if(appLogo->texture()){ImGui::Image(static_cast<ImTextureID>(appLogo->texture()),cyui::dp(44,44));ImGui::SameLine(0,cyui::dp(10));}
            ImGui::BeginGroup();cyui::heading("CyVR QuestDev",2);cyui::textFaint("OUTILS DÉVELOPPEUR / META QUEST");ImGui::EndGroup();
            ImGui::SameLine(0,cyui::dp(16));
            const char* pages[]={"Console","Applications","Réglages","Appareil","ADB","Plugins","Assistant IA","Fichiers","Métriques"};cyui::segmented("##pages",pages,9,&page,cyui::dp(740));
            ImGui::SameLine();cyui::alignRight(cyui::dp(300));
            ImGui::SetNextItemWidth(cyui::dp(255));
            std::string label=serial.empty()?"Sélectionner un casque":serial;
            for(const auto& d:devices)if(d.serial==serial)label=(d.model.empty()?d.serial:d.model)+" · "+d.state;
            if(ImGui::BeginCombo("##devices",label.c_str())) {
                for(const auto& d:devices) {
                    std::string name=(d.model.empty()?d.serial:d.model)+" / "+d.serial+" · "+d.state;
                    cyui::ScopedDisabled disabled(options.demo || d.state!="device");
                    if(ImGui::Selectable(name.c_str(),d.serial==serial))selectDevice(d.serial);
                }
                if(devices.empty())cyui::textDim("Aucun casque USB ou Wi-Fi.");
                ImGui::EndCombo();
            }
            ImGui::SameLine();cyui::ScopedDisabled disabled(options.demo);
            if(cyui::iconButton(cyui::icon::refresh,"Actualiser les appareils (F5)"))backend->refresh();
        }cyui::endTopBar();
    }
    void statusbar() {
        if(cyui::beginStatusBar("##status")) {
            cyui::statusDot(capturing?cyui::palette().success:cyui::palette().warning);
            ImGui::SameLine();
            cyui::statusText(capturing?cyui::Tone::success:cyui::Tone::warning,"%s",options.demo?"DÉMONSTRATION":capturing?"DIRECT":online?"CONNECTÉ":"HORS LIGNE");
            ImGui::SameLine(0,cyui::dp(18));cyui::textFaint("%zu / %zu lignes · %.1f / %zu Mo",store.entries().size(),store.capacity(),double(store.bytes())/(1024*1024),store.byteLimit()/(1024*1024));
            cyui::tooltip("Configurer les limites du journal (clic)");if(ImGui::IsItemClicked()){page=2;settingsSection=2;}
            ImGui::SameLine(0,cyui::dp(18));cyui::textFaint("%s",trim(status.substr(0,105)).c_str());
            auto lost=store.discarded()+(backend?backend->dropped():0);
            if(lost){ImGui::SameLine();cyui::statusText(cyui::Tone::warning,"%llu lignes retirées / perdues",static_cast<unsigned long long>(lost));}
        }cyui::endStatusBar();
    }
    void applicationList() {
        cyui::overline("APPLICATIONS");cyui::space(5);
        if(cyui::searchField("##apps",appSearch.data(),int(appSearch.size()),"Rechercher un package",-1)){}
        const char* modes[]={"Actives","Installées","Système"};cyui::segmented("##appsMode",modes,3,&appMode,-1);
        cyui::space(8);
        if(cyui::sidebarItem(cyui::icon::library,"Toutes les applications",view.package.empty()))openApp("");
        if(cyui::sidebarItem(cyui::icon::code,"Système / non attribué",view.package=="@system"))openApp("@system");
        cyui::divider();
        ImGui::BeginChild("##appList",ImVec2(0,-cyui::dp(90)));
        int shown=0;std::string query=lower(appSearch.data());
        auto sorted=packages;std::sort(sorted.begin(),sorted.end(),[&](const Package& a,const Package& b){const auto& x=counts[a.name];const auto& y=counts[b.name];if((x.processes>0)!=(y.processes>0))return x.processes>0;if(x.lines!=y.lines)return x.lines>y.lines;return a.name<b.name;});
        for(const auto& p:sorted) {
            auto c=counts[p.name];if(appMode==0 && (c.processes==0 || p.system))continue;if(appMode==1 && p.system)continue;if(appMode==2 && !p.system)continue;
            if(!query.empty() && lower(p.name).find(query)==std::string::npos)continue;
            ++shown;ImGui::PushID(p.name.c_str());
            auto pos=ImGui::GetCursorPos();bool selected=view.package==p.name;
            if(ImGui::Selectable("##app",selected,0,ImVec2(0,cyui::dp(59))))openApp(p.name);
            cyui::tooltip(p.name.c_str());auto after=ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(pos.x+cyui::dp(9),pos.y+cyui::dp(7)));
            cyui::statusDot(c.processes?cyui::palette().success:cyui::palette().textFaint);ImGui::SameLine();
            ImGui::TextUnformatted(shortName(p.name).c_str());
            ImGui::SetCursorPosX(pos.x+cyui::dp(9));
            cyui::textFaint("%d processus · %d logs",c.processes,c.lines);
            if(c.errors){ImGui::SameLine();cyui::textColoured(cyui::palette().error,"%d E",c.errors);}
            ImGui::SetCursorPos(after);ImGui::Dummy(ImVec2(0,0));ImGui::PopID();
        }
        if(!shown) {
            cyui::space(12);cyui::textDim("%s",online?"Aucune application correspondante.":"Connectez un casque pour\nvoir ses applications.");
            if(appMode==0) {cyui::space(8);if(cyui::ghostButton("Voir les applications installées"))appMode=1;}
        }
        ImGui::EndChild();cyui::divider();
        cyui::textFaint("%d applications · %zu processus",shown,store.processes().size());
        cyui::ScopedDisabled disabled(!online||options.demo);
        if(cyui::primaryButton("Installer un APK",ImVec2(-1,0)))install();
    }
    void viewTabs() {
        ImGui::SetNextItemWidth(cyui::dp(200));
        if(ImGui::BeginTabBar("##appTabs",ImGuiTabBarFlags_FittingPolicyScroll)) {
            const auto requested=view.package;
            bool closedActive=false;
            auto tab=[&](const std::string& p,const std::string& title,bool* open) {
                ImGuiTabItemFlags flags=tabRequest && requested==p?ImGuiTabItemFlags_SetSelected:0;
                if(ImGui::BeginTabItem((title+"###"+p).c_str(),open,flags)) { if((!tabRequest || requested==p) && view.package!=p){view.package=p;view.dirty=true;}ImGui::EndTabItem(); }
                cyui::tooltip(p.empty()?"Tous les messages":p=="@system"?"Messages sans application identifiée":p.c_str());
            };
            tab("","Tout",nullptr);tab("@system","Système",nullptr);
            for(auto it=tabs.begin();it!=tabs.end();) { bool open=true;tab(*it,shortName(*it),&open);if(!open){if(view.package==*it){openApp("");closedActive=true;}it=tabs.erase(it);}else ++it; }
            ImGui::EndTabBar();
            tabRequest=closedActive;
        }
    }
    void toolbar(View& v,bool compact) {
        ImGui::PushID(&v);
        if(cyui::searchField("##text",v.search.data(),int(v.search.size()),"Chercher dans les messages…",std::max(cyui::dp(130),ImGui::GetContentRegionAvail().x-cyui::dp(compact?235.0f:390.0f))))v.dirty=true;
        ImGui::SameLine();
        const char* levels[]={"Verbose +","Debug +","Info +","Warning +","Error +","Fatal"};
        if(cyui::combo("##level",&v.minimum,levels,6,cyui::dp(105)))v.dirty=true;
        ImGui::SameLine();
        if(cyui::iconButton(v.paused?cyui::icon::play:cyui::icon::pause,"Suspendre / reprendre l’affichage (Espace)",v.paused))pause(v);
        ImGui::SameLine();if(cyui::iconButton(cyui::icon::download,"Exporter la vue filtrée (Ctrl+S)"))ImGui::OpenPopup("##export");
        if(ImGui::BeginPopup("##export")){if(ImGui::MenuItem("Texte (.log)"))exportView(v,false);if(ImGui::MenuItem("JSON Lines (.jsonl)"))exportView(v,true);ImGui::EndPopup();}
        if(!compact) {
            ImGui::SameLine();if(cyui::iconButton(cyui::icon::copy,"Copier toutes les lignes filtrées (Ctrl+Maj+C)"))ImGui::SetClipboardText(viewText(v).c_str());
            ImGui::SameLine();if(cyui::iconButton(cyui::icon::trash,"Vider le journal local (Ctrl+L)")){store.clear();detail.reset();}
        }
        ImGui::SameLine();if(cyui::iconButton(cyui::icon::filter,"Filtres avancés (clic)",v.exclude[0]||v.tag[0]||v.pid>0||v.events))ImGui::OpenPopup("##filters");
        if(ImGui::BeginPopup("##filters")) {
            cyui::sectionTitle("Filtres avancés");
            if(cyui::beginProperties("##filterProps",cyui::dp(115))) {
                cyui::property("Exclure");if(ImGui::InputText("##exclude",v.exclude.data(),v.exclude.size()))v.dirty=true;
                cyui::property("Tag");if(ImGui::InputText("##tag",v.tag.data(),v.tag.size()))v.dirty=true;
                cyui::property("PID (0 = tous)");if(ImGui::InputInt("##pid",&v.pid))v.dirty=true;
                cyui::property("Événements Android");if(cyui::toggleSwitch("##events",&v.events))v.dirty=true;
                cyui::endProperties();
            }
            cyui::textFaint("Recherche textuelle sans distinction de casse.");
            if(cyui::ghostButton("Réinitialiser les filtres")){v.search.fill(0);v.tag.fill(0);v.exclude.fill(0);v.pid=0;v.minimum=0;v.events=false;v.dirty=true;}
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    void rawLogs(View& v,float footer){
        if(v.rawInvalidate){clearRawLogSelection();v.rawInvalidate=false;v.rawTail=true;v.rawRevision=~uint64_t(0);v.lastScroll=-1;}
        bool selected=rawLogSelection();
        auto rebuild=[&]{
            v.rawEnd=v.ids.size();if(!v.rawTail){auto end=std::upper_bound(v.ids.begin(),v.ids.end(),v.rawAnchor);v.rawEnd=size_t(end-v.ids.begin());}
            v.rawFirst=v.rawEnd;size_t bytes=0;
            while(v.rawFirst>0&&v.rawEnd-v.rawFirst<10000){auto* e=find(v.ids[v.rawFirst-1]);if(!e)break;auto n=rawLog(*e).size()+1;if(bytes+n>2*1024*1024&&bytes)break;bytes+=n;--v.rawFirst;}
            v.rawText.clear();v.rawText.reserve(bytes);for(size_t i=v.rawFirst;i<v.rawEnd;++i)if(auto* e=find(v.ids[i])){v.rawText+=rawLog(*e);v.rawText+='\n';}
            v.rawRevision=store.revision();
        };
        if(!selected&&v.rawRevision!=store.revision())rebuild();
        {cyui::ScopedDisabled disabled(v.rawFirst==0||selected);if(cyui::button("Précédentes")){v.rawTail=false;v.rawAnchor=v.ids[v.rawFirst-1];v.follow=false;v.lastScroll=-1;rebuild();}}
        ImGui::SameLine();{cyui::ScopedDisabled disabled(v.rawEnd>=v.ids.size()||selected);if(cyui::button("Suivantes")){size_t end=v.rawEnd,bytes=0;while(end<v.ids.size()&&end-v.rawEnd<10000){auto* entry=find(v.ids[end]);if(!entry)break;auto n=rawLog(*entry).size()+1;if(bytes+n>2*1024*1024&&bytes)break;bytes+=n;++end;}v.rawTail=end==v.ids.size();if(end)v.rawAnchor=v.ids[end-1];v.lastScroll=-1;rebuild();}}
        ImGui::SameLine();cyui::textFaint("RAW %zu–%zu / %zu",v.rawEnd?v.rawFirst+1:0,v.rawEnd,v.ids.size());ImGui::SameLine();cyui::helpMarker("Sélection à la souris, Ctrl+A pour la page, Ctrl+C pour copier. Les boutons Copier et Exporter incluent toute la vue filtrée.");
        if(selected){ImGui::SameLine();if(cyui::ghostButton("Libérer la sélection")){clearRawLogSelection();v.rawRevision=~uint64_t(0);}}
        cyui::ScopedFont font(cyui::monoFont());v.rawSelected=drawRawLog(v.rawText,v.follow,v.paused,v.lastScroll,ImVec2(-1,-footer),cyui::dp(2));
        if(v.follow&&!v.rawTail){v.rawTail=true;v.rawRevision=~uint64_t(0);}
        if(!v.follow&&v.rawTail&&v.rawEnd){v.rawTail=false;v.rawAnchor=v.ids[v.rawEnd-1];}
    }
    void logTable(View& v,bool compact) {
        ImGui::PushID(&v);updateView(v);
        auto title=v.package.empty()?"Tous les messages":v.package=="@system"?"Système / non attribué":v.package;
        cyui::textDim("%s",title.c_str());
        ImGui::SameLine();const char* modes[]={"Colonnes","RAW"};int mode=v.rawMode?1:0;
        if(cyui::segmented("##logMode",modes,2,&mode,cyui::dp(160))){v.rawMode=mode==1;v.dirty=true;v.lastScroll=-1;v.rawTail=true;updateView(v);}
        cyui::tooltip("RAW : texte Logcat original sélectionnable. Colonnes : cliquer un en-tête pour trier, Maj pour combiner les tris.");
        toolbar(v,compact);
        if(v.paused)cyui::callout(cyui::Tone::warning,"Affichage en pause · la capture continue. Le tampon reste limité.");
        bool wasFollowing=v.follow;
        auto flags=ImGuiTableFlags_ScrollY|ImGuiTableFlags_ScrollX|ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_Resizable|ImGuiTableFlags_Reorderable|ImGuiTableFlags_Hideable|ImGuiTableFlags_Sortable|ImGuiTableFlags_SortMulti;
        float footer=ImGui::GetFrameHeightWithSpacing();
        if(v.ids.empty()) {
            ImGui::BeginChild("##empty",ImVec2(0,-footer));
            cyui::emptyState(store.entries().empty()?"En attente de messages":"Aucun message avec ces filtres",store.entries().empty()?(online?"Lancez une application sur le casque. Les messages apparaîtront ici.":"Branchez le Quest en USB, activez le mode développeur et acceptez le débogage dans le casque."):"Modifiez la recherche, la sévérité ou l’application sélectionnée.",cyui::icon::document);
            ImGui::EndChild();
        } else if(v.rawMode)rawLogs(v,footer);
        else if(ImGui::BeginTable("##log",compact?5:6,flags,ImVec2(0,-footer))) {
            ImGui::TableSetupScrollFreeze(0,1);
            auto columnFlags=[&](LogColumn column){return ImGuiTableColumnFlags_WidthFixed|(int(column)==options.logSort?ImGuiTableColumnFlags_DefaultSort|(options.logDescending?ImGuiTableColumnFlags_PreferSortDescending:0):0);};
            ImGui::TableSetupColumn("Heure locale",columnFlags(LogColumn::time),cyui::dp(104),unsigned(LogColumn::time));
            ImGui::TableSetupColumn("Niv.",columnFlags(LogColumn::level),cyui::dp(36),unsigned(LogColumn::level));
            ImGui::TableSetupColumn("PID / TID",columnFlags(LogColumn::pid),cyui::dp(85),unsigned(LogColumn::pid));
            if(!compact)ImGui::TableSetupColumn("Application",columnFlags(LogColumn::application),cyui::dp(160),unsigned(LogColumn::application));
            ImGui::TableSetupColumn("Tag",columnFlags(LogColumn::tag),cyui::dp(compact?100.0f:135.0f),unsigned(LogColumn::tag));
            ImGui::TableSetupColumn("Message",columnFlags(LogColumn::message),cyui::dp(compact?620.0f:950.0f),unsigned(LogColumn::message));
            ImGui::TableHeadersRow();
            if(auto* specs=ImGui::TableGetSortSpecs();specs&&specs->SpecsDirty){v.sort.clear();for(int i=0;i<specs->SpecsCount;++i){const auto& s=specs->Specs[i];v.sort.push_back({LogColumn(s.ColumnUserID),s.SortDirection==ImGuiSortDirection_Descending});}v.dirty=true;updateView(v);specs->SpecsDirty=false;v.lastScroll=-1;}
            {
            cyui::ScopedFont font(cyui::monoFont());
            ImGuiListClipper clipper;clipper.Begin(int(v.ids.size()));
            while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                auto* e=find(v.ids[size_t(i)]);if(!e)continue;
                ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::PushID(int(e->id));
                if(ImGui::Selectable(clockText(e->epoch).c_str(),v.selected==e->id,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)) {v.selected=e->id;detail=*e;if(ImGui::IsMouseDoubleClicked(0))detailsOpen=true;}
                if(ImGui::BeginPopupContextItem("##rowMenu")) {if(ImGui::MenuItem("Copier la ligne"))ImGui::SetClipboardText(formatLog(*e).c_str());if(ImGui::MenuItem("Inspecter")){detail=*e;detailsOpen=true;}ImGui::EndPopup();}
                ImGui::TableNextColumn();const auto& p=cyui::palette();auto color=severity(e->level)>=4?p.error:e->level=='W'?p.warning:e->level=='D'||e->level=='V'?p.textFaint:p.text;
                cyui::textColoured(color,"%c",e->level);
                ImGui::TableNextColumn();cyui::textFaint("%d/%d",e->pid,e->tid);
                if(!compact){ImGui::TableNextColumn();ImGui::TextUnformatted(e->package.empty()?"—":shortName(e->package).c_str());cyui::tooltip(e->process.empty()?"Origine non attribuée":e->process.c_str());}
                ImGui::TableNextColumn();cyui::textDim("%s",e->tag.c_str());
                ImGui::TableNextColumn();ImGui::TextUnformatted(e->message.c_str());cyui::tooltip(e->message.c_str());
                ImGui::PopID();
            }
            }
            if(v.sort.size()==1&&v.sort[0].column==LogColumn::time&&!v.sort[0].descending)followLogViewport(v.follow,v.paused,v.lastScroll,cyui::dp(2));else v.follow=false;
            ImGui::EndTable();
        }
        cyui::textFaint("%zu lignes",v.ids.size());ImGui::SameLine(0,cyui::dp(20));
        {bool chronological=v.rawMode||(v.sort.size()==1&&v.sort[0].column==LogColumn::time&&!v.sort[0].descending);cyui::ScopedDisabled disabled(!chronological);if(cyui::toggleSwitch("Suivre le direct",&v.follow)&&v.follow){clearRawLogSelection();v.rawTail=true;v.rawRevision=~uint64_t(0);v.lastScroll=-1;}}
        cyui::tooltip("Le suivi se suspend en remontant et reprend au retour en bas. En colonnes, triez par Heure locale croissante pour suivre le direct.");
        if(!wasFollowing&&v.follow)v.dirty=true;
        if(v.selected){ImGui::SameLine();if(cyui::ghostButton("Inspecter")){if(auto* e=find(v.selected))detail=*e;detailsOpen=true;}}
        ImGui::PopID();
    }
    void console() {
        cyui::panelHeader("Console", "Un journal par application. Tous les processus, une seule capture.");
        ImGui::SameLine();cyui::alignRight(cyui::dp(145));ImGui::Checkbox("Côte à côte",&compare);
        if(options.demo)cyui::callout(cyui::Tone::info,"Mode démonstration · les applications et les messages sont simulés.");
        viewTabs();
        if(compare) {
            auto selector=[&](View& selected,bool main) {
                ImGui::PushID(&selected);ImGui::SetNextItemWidth(-1);std::string label=selected.package.empty()?"Toutes les applications":selected.package=="@system"?"Système / non attribué":selected.package;
                if(ImGui::BeginCombo("##appSelect",label.c_str())) {
                    auto choose=[&](const std::string& p,const char* name){if(ImGui::Selectable(name,selected.package==p)){if(main)openApp(p);else{selected.package=p;selected.dirty=true;}}};
                    choose("","Toutes les applications");choose("@system","Système / non attribué");
                    for(const auto& p:packages)choose(p.name,p.name.c_str());ImGui::EndCombo();
                }ImGui::PopID();
            };
            if(ImGui::BeginTable("##compare",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableNextColumn();ImGui::BeginChild("##left");selector(view,true);logTable(view,true);ImGui::EndChild();
                ImGui::TableNextColumn();ImGui::BeginChild("##right");
                selector(second,false);
                logTable(second,true);ImGui::EndChild();ImGui::EndTable();
            }
        } else logTable(view,false);
    }
    void applications() {
        cyui::panelHeader("Applications", "Lancer, arrêter et installer depuis le casque sélectionné.");cyui::space(10);
        cyui::searchField("##managementSearch",managementSearch.data(),int(managementSearch.size()),"Rechercher une application",-1);
        std::vector<const Package*> shown;size_t categoryCount=0;auto search=lower(managementSearch.data());
        for(const auto& p:packages){if(applicationCategory==0&&p.system)continue;if(applicationCategory==1&&counts[p.name].processes==0)continue;if(applicationCategory==2&&!p.system)continue;++categoryCount;if(lower(p.name).find(search)!=std::string::npos)shown.push_back(&p);}
        cyui::textDim("%zu affichée(s) / %zu dans cette catégorie · %zu applications installées",shown.size(),categoryCount,packages.size());
        if(packages.empty()){cyui::emptyState("Aucune application disponible","Connectez un casque autorisé pour charger les packages.",cyui::icon::library);return;}
        if(shown.empty()){cyui::emptyState("Aucune application dans cette liste","Modifiez la catégorie ou la recherche.",cyui::icon::search);return;}
        if(ImGui::BeginTable("##packages",5,ImGuiTableFlags_ScrollY|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable,ImVec2(0,-cyui::dp(75)))) {
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableSetupColumn("Package",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("État",ImGuiTableColumnFlags_WidthFixed,cyui::dp(100));ImGui::TableSetupColumn("UID",ImGuiTableColumnFlags_WidthFixed,cyui::dp(65));ImGui::TableSetupColumn("Logs",ImGuiTableColumnFlags_WidthFixed,cyui::dp(65));ImGui::TableSetupColumn("Actions",ImGuiTableColumnFlags_WidthFixed,cyui::dp(205));ImGui::TableHeadersRow();
            for(const auto* item:shown) {const auto& p=*item;
                auto c=counts[p.name];ImGui::PushID(p.name.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(p.name.c_str());
                ImGui::TableNextColumn();cyui::badge(c.processes?"Active":p.system?"Système":"Installée",c.processes?cyui::Tone::success:cyui::Tone::neutral);
                ImGui::TableNextColumn();cyui::textDim("%d",p.uid);ImGui::TableNextColumn();cyui::textDim("%d",c.lines);ImGui::TableNextColumn();
                if(cyui::ghostButton("Logs")){openApp(p.name);page=0;}ImGui::SameLine();
                {cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::iconButton(cyui::icon::play,"Lancer l’application (clic)"))backend->action("launch",p.name);ImGui::SameLine();if(cyui::iconButton(cyui::icon::stop,"Forcer l’arrêt de l’application (clic)")){pendingStop=p.name;confirmStop=true;}}
                ImGui::PopID();
            }ImGui::EndTable();
        }
        if(!lastOperation.empty()){cyui::textDim("Dernière opération");ImGui::TextWrapped("%s",lastOperation.substr(0,500).c_str());}
    }
    void contextSidebar() {
        if(page==0){applicationList();return;}
        auto item=[&](const char* icon,const char* label,int& selected,int n){if(cyui::sidebarItem(icon,label,selected==n))selected=n;};
        if(page==1){
            size_t userCount=0,systemCount=0,activeCount=0;for(const auto& p:packages){if(p.system)++systemCount;else ++userCount;if(counts[p.name].processes)++activeCount;}
            auto category=[&](const char* icon,const char* name,int value,size_t count){auto badge=std::to_string(count);if(cyui::sidebarItem(icon,name,applicationCategory==value,badge.c_str()))applicationCategory=value;};
            cyui::overline("GESTION DES APPLICATIONS");cyui::space(10);category(cyui::icon::library,"Apps utilisateur",0,userCount);category(cyui::icon::play,"En cours d’exécution",1,activeCount);category(cyui::icon::code,"Packages système",2,systemCount);category(cyui::icon::viewAll,"Tous les packages",3,packages.size());
            cyui::divider();{cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::primaryButton("Installer un APK",ImVec2(-1,0)))install();if(cyui::button("Actualiser les packages",cyui::Variant::secondary,ImVec2(-1,0)))backend->refresh();}
            cyui::space(10);cyui::textFaint("%zu packages détectés",packages.size());
        }else if(page==3){
            cyui::overline("APPAREIL");cyui::space(10);item(cyui::icon::home,"Device Actions",deviceSection,0);item(cyui::icon::video,"Capture et diffusion",deviceSection,1);item(cyui::icon::clock,"Performance et traces",deviceSection,2);item(cyui::icon::settings,"Runtime et connexion",deviceSection,3);
            cyui::divider();cyui::textDim("%s",serial.empty()?"Aucun casque sélectionné":serial.c_str());cyui::textFaint("%s",online?"Connexion ADB active":"Hors ligne");
        }else if(page==4){
            cyui::overline("TERMINAL ADB");cyui::space(10);cyui::textFaint("Commandes rapides");
            const std::pair<const char*,const char*> commands[]={{"Propriétés Android","getprop"},{"Batterie","dumpsys battery"},{"Stockage","df -h /sdcard"},{"Processus","ps -A -o PID,UID,NAME"},{"Activité courante","dumpsys activity activities"},{"Liaisons TCP","forward --list"}};
            for(const auto& preset:commands)if(cyui::sidebarItem(cyui::icon::code,preset.first,false)){strncpy_s(command.data(),command.size(),preset.second,_TRUNCATE);terminalMode=std::string(preset.second).rfind("forward",0)==0?1:0;}
            cyui::divider();cyui::textFaint("HISTORIQUE");
            for(auto it=taskOrder.rbegin();it!=taskOrder.rend();++it){auto& t=tasks[*it];if(t.kind=="info")continue;ImGui::PushID(int(t.id));if(ImGui::Selectable(((t.done?(t.ok?"OK  ":"ERR "):"…  ")+t.label).c_str(),selectedTask==t.id))selectedTask=t.id;ImGui::PopID();}
        }else if(page==5){
            cyui::overline("PLUGINS");cyui::space(10);if(cyui::button("Créer un plugin",cyui::Variant::secondary,ImVec2(-1,0)))newPlugin=true;if(cyui::button("Recharger",cyui::Variant::secondary,ImVec2(-1,0)))reloadPlugins();if(cyui::ghostButton("Ouvrir le dossier utilisateur")){auto dir=userRoot()/L"plugins";std::filesystem::create_directories(dir);openLocal(dir);}cyui::divider();
            for(size_t i=0;i<plugins.size();++i){auto& p=plugins[i];ImGui::PushID(int(i));if(cyui::sidebarItem(cyui::icon::library,p.name.empty()?p.path.filename().string().c_str():p.name.c_str(),selectedPlugin==int(i),!p.error.empty()?"!":configurablePlugin(p)?"HUD":"ADB"))selectedPlugin=int(i);ImGui::PopID();}
        }else if(page==2){
            cyui::overline("RÉGLAGES");cyui::space(10);item(cyui::icon::settings,"Général",settingsSection,0);item(cyui::icon::document,"Limites du journal",settingsSection,2);item(cyui::icon::download,"Mises à jour GitHub",settingsSection,1);cyui::divider();if(cyui::sidebarItem(cyui::icon::code,"MCP / SDK et IA",false))page=6;cyui::textFaint("CyUI · Dear ImGui\nCyToolsCore · MIT");
        }else if(page==8){metrics->sidebar();
        }else if(page==7){
            files->sidebar(packages,view.package);
        }else {
            cyui::overline("ASSISTANT IA");cyui::space(10);
            if(cyui::button("Nouvelle discussion",cyui::Variant::secondary,ImVec2(-1,0))){if(assistantReady&&!assistantBusy){assistantBusy=true;assistant->send({{"type","new_chat"}});}}
            if(cyui::ghostButton("Ouvrir en fenêtre séparée"))floatingAssistant=true;
            cyui::divider();cyui::sectionTitle("Contexte partagé");ImGui::Checkbox("Contexte de QuestDev",&sendContext);ImGui::Checkbox("Derniers logs de la vue",&sendLogs);cyui::textFaint("50 messages maximum dans le contexte.\nLes outils peuvent lire davantage de logs.");
            if(cyui::button("Inspecter le contexte envoyé"))ImGui::OpenPopup("##contextPreview");
            if(ImGui::BeginPopup("##contextPreview")){auto text=contextSnapshot().dump(2,' ',false,Json::error_handler_t::replace);ImGui::BeginChild("##contextContent",cyui::dp(700,450),ImGuiChildFlags_Borders);ImGui::TextWrapped("%s",text.c_str());ImGui::EndChild();if(cyui::button("Copier"))ImGui::SetClipboardText(text.c_str());ImGui::EndPopup();}
            cyui::divider();cyui::sectionTitle("Permissions des outils");const char* policies[]={"Lecture seule","Demander avant action","Autoriser les actions"};cyui::combo("##aiPolicy",&aiPolicy,policies,3,-1);
            cyui::space(10);cyui::textFaint("Même politique pour l’assistant\net les clients MCP / SDK.");
            cyui::divider();if(cyui::button("Configuration MCP"))ImGui::OpenPopup("##mcpConfig");
            if(ImGui::BeginPopup("##mcpConfig")){cyui::textDim("%s",mcpConfig.empty()?"Démarrez le runtime IA.":mcpConfig.c_str());if(!mcpConfig.empty()){std::ifstream in(widen(mcpConfig));std::ostringstream contents;contents<<in.rdbuf();if(cyui::button("Copier le JSON MCP"))ImGui::SetClipboardText(contents.str().c_str());ImGui::TextWrapped("%s",contents.str().c_str());}ImGui::EndPopup();}
        }
    }
    void deviceActions() {
        cyui::panelHeader("Device Actions","Captures, développement, performances et contrôles du Quest sélectionné.");cyui::space(10);
        if(!online&&!options.demo)cyui::callout(cyui::Tone::warning,"Sélectionnez un casque autorisé pour utiliser ses actions.");
        auto value=[&](const char* key,const char* fallback="Non lu"){auto it=deviceInfo.find(key);return it==deviceInfo.end()||it->second.empty()?std::string(fallback):it->second;};
        if(cyui::beginCard("##deviceInfo")){
            cyui::heading(value("MODEL",options.demo?"Meta Quest 3 · démonstration":"Quest").c_str(),2);cyui::textDim("Android %s · Batterie %s %% · IP %s",value("OS","?").c_str(),value("BATTERY","?").c_str(),value("IP","USB").c_str());
            cyui::textFaint("Stockage : %s",value("STORAGE").c_str());ImGui::SameLine();{cyui::ScopedDisabled disabled(!online||options.demo||infoTask!=0);if(cyui::ghostButton("Actualiser l’état"))refreshDevice(true);}
        }cyui::endCard();cyui::space(10);
        const std::map<std::string,const char*> icons={{"##cast",cyui::icon::video},{"##screen",cyui::icon::camera},{"##record",cyui::icon::record},{"##browser",cyui::icon::globe},{"##gpu",cyui::icon::code},{"##perfetto",cyui::icon::clock},{"##hud",cyui::icon::view},{"##metricsRecord",cyui::icon::equalizer},{"##wifi",cyui::icon::connect},{"##prox",cyui::icon::touch},{"##boundary",cyui::icon::crop},{"##power",cyui::icon::power},{"##runtime",cyui::icon::bug}};
        auto card=[&](const char* id,const char* title,const std::function<void()>& draw){if(cyui::beginCard(id)){auto heading=std::string(cyui::iconsAvailable()?icons.at(id):"")+"  "+title;cyui::heading(heading.c_str(),3);cyui::space(8);draw();}cyui::endCard();};
        auto flag=[&](const char* key,bool inverted=false)->std::optional<bool>{auto v=value(key,"");if(v=="1")return !inverted;if(v=="0")return inverted;return {};};
        auto ovrFlag=[&](const char* key)->std::optional<bool>{if(options.demo)return std::string(key)=="overlay";for(const auto& [id,panel]:pluginPanels)if(panel.controller=="ovr_metrics"&&panel.observed.contains("configuration")){const auto& config=panel.observed["configuration"];if(config.contains(key)&&config[key].is_boolean())return config[key].get<bool>();}return {};};
        auto toggle=[&](const char* id,const char* caption,std::optional<bool> observed,const std::function<void(bool)>& change){bool enabled=observed.value_or(deviceRequests.count(id)?deviceRequests[id]:false);ImGui::PushID(id);{cyui::ScopedDisabled disabled(!online||options.demo||deviceCommandBusy()||infoTask!=0||pluginBusy());if(cyui::toggleSwitch(caption,&enabled)){deviceRequests[id]=enabled;change(enabled);}}if(!observed)cyui::textFaint("%s",deviceRequests.count(id)?(deviceRequests[id]?"Activation demandée · état non vérifié":"Désactivation demandée · état non vérifié"):"État non lu · actualisez pour le vérifier");else cyui::textDim("%s",*observed?"Activé":"Désactivé");ImGui::PopID();};
        if(ImGui::BeginTable("##actionsGrid",3,ImGuiTableFlags_SizingStretchSame)) {
            if(deviceSection==0||deviceSection==1){
                ImGui::TableNextColumn();card("##cast","MIRRORING VIDÉO / AUDIO",[&]{auto video=videoMirror->state();cyui::textDim("Flux H.264 · fenêtre vidéo native");if(video.active){cyui::textDim("%s",video.status.c_str());if(video.width)cyui::textMono("%d × %d · %d FPS",video.width,video.height,video.fps);{cyui::ScopedDisabled disabled(video.stopping);if(cyui::button("Arrêter et finaliser"))mirrorUi.push_back(videoMirror->stop());}ImGui::SameLine();if(cyui::ghostButton("Voir la vidéo"))videoMirror->focus();}else{cyui::textFaint("Qualité, audio PC, plein écran et MKV.");{cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::button("Démarrer le mirroring…"))mirrorSetup=true;}}if(cyui::ghostButton("Réglages / journal"))mirrorSetup=true;if(!video.error.empty())cyui::textDim("%s",video.error.c_str());});
                ImGui::TableNextColumn();card("##screen","CAPTURE D’ÉCRAN",[&]{cyui::textDim("PNG du contenu du casque.");{cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::button("Enregistrer une capture…")){auto path=fileDialog(true,L"Image PNG\0*.png\0\0",L"png",L"quest-capture.png");if(!path.empty())submit("Capture d’écran",{{{"exec-out","screencap","-p"},path}},"screenshot",path);}}});
                ImGui::TableNextColumn();card("##record","ENREGISTREMENT VIDÉO",[&]{ImGui::SetNextItemWidth(cyui::dp(115));ImGui::InputInt("secondes##video",&recordSeconds);recordSeconds=std::clamp(recordSeconds,1,180);{cyui::ScopedDisabled disabled(!online||options.demo||recordTask!=0);if(cyui::button("Enregistrer en MP4…")){auto path=fileDialog(true,L"Vidéo MP4\0*.mp4\0\0",L"mp4",L"quest-recording.mp4");if(!path.empty()){auto job=videoJob(path,recordSeconds);recordTask=submit(job.label,job.steps,"record",path,job.timeoutMs,job.cleanupSteps);}}}if(recordTask){ImGui::SameLine();if(cyui::button("Terminer"))shellAction("Finaliser la vidéo",finishVideoCommand());}});
                ImGui::TableNextColumn();card("##browser","NAVIGATEUR",[&]{cyui::textField("##browserUrl",browserUrl.data(),int(browserUrl.size()),"https://…",-1);{cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::button("Ouvrir sur le casque")){auto url=trim(browserUrl.data());if(url.rfind("https://",0)==0||url.rfind("http://",0)==0)shellAction("Ouvrir le navigateur","am start -a android.intent.action.VIEW -d "+shellQuote(url));else cyui::notify("Entrez une URL HTTP ou HTTPS.",cyui::Tone::error);}}});
            }
            if(deviceSection==0||deviceSection==2){
                ImGui::TableNextColumn();card("##gpu","GPU PROFILING",[&]{toggle("gpu","Profiling GPU",flag("GPU"),[&](bool on){shellAction(on?"Activer le profiling GPU":"Désactiver le profiling GPU",std::string("setprop debug.vr.gpuprofilingservice ")+(on?"1":"0"));});cyui::textFaint("Détails et compteurs via ADB.");});
                ImGui::TableNextColumn();card("##perfetto","TRACE PERFETTO",[&]{ImGui::SetNextItemWidth(cyui::dp(115));ImGui::InputInt("secondes##trace",&traceSeconds);traceSeconds=std::clamp(traceSeconds,1,120);{cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::button("Enregistrer une trace…")){auto path=fileDialog(true,L"Trace Perfetto\0*.perfetto-trace\0\0",L"perfetto-trace",L"quest.perfetto-trace");if(!path.empty()){auto remote="/data/misc/perfetto-traces/questdev-"+std::to_string(GetTickCount64())+".perfetto-trace";submit("Trace Perfetto",{{{"shell","perfetto","-o",remote,"-t",std::to_string(traceSeconds)+"s","-b","32mb","sched","freq","idle","am","wm","gfx","view","binder_driver"},{}},{{"pull",remote,narrow(path)},{}},{{"shell","rm -f "+remote},{}}},"trace",path,(traceSeconds+30)*1000);}}}});
                ImGui::TableNextColumn();card("##hud","METRICS HUD",[&]{toggle("hud","Afficher le HUD",ovrFlag("overlay"),[&](bool on){shellAction(on?"Afficher les métriques":"Masquer les métriques",on?metricsCommand("ENABLE_STATS")+"; "+metricsCommand("ENABLE_OVERLAY"):metricsCommand("DISABLE_OVERLAY"));});cyui::textFaint("Nécessite le service OVR Metrics.");});
                ImGui::TableNextColumn();card("##metricsRecord","METRICS RECORDING",[&]{toggle("csv","Enregistrer en CSV",ovrFlag("csv"),[&](bool on){shellAction(on?"Enregistrer les métriques":"Arrêter les métriques",metricsCommand(on?"ENABLE_CSV":"DISABLE_CSV"));});{cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::iconTextButton(cyui::icon::download,"Récupérer les CSV…")){auto folder=folderDialog();if(!folder.empty()){auto destination=std::filesystem::path(folder)/(L"QuestMetrics-"+std::to_wstring(GetTickCount64()));submit("Récupérer les métriques",{{{"pull","/sdcard/Android/data/com.oculus.ovrmonitormetricsservice/files/CapturedMetrics",narrow(destination.wstring())},{}}},"metrics",destination.wstring());}}}});
            }
            if(deviceSection==0||deviceSection==3){
                ImGui::TableNextColumn();card("##wifi","ADB OVER WI-FI",[&]{auto port=value("WIFI","");std::optional<bool> wifi;if(port=="-1"||port=="0")wifi=false;else if(!port.empty()&&std::all_of(port.begin(),port.end(),[](char c){return c>='0'&&c<='9';}))wifi=true;toggle("wifi","ADB sur Wi-Fi",wifi,[&](bool on){if(on)submit("ADB Wi-Fi 5555",{{{"tcpip","5555"},{}}},"device:control");else submit("ADB USB",{{{"usb"},{}}},"device:control");});cyui::textField("##deviceNetwork",network.data(),int(network.size()),"IP:5555",-1);{cyui::ScopedDisabled disabled(options.demo);if(cyui::button("Connecter en Wi-Fi"))backend->action("connect",network.data());}});
                ImGui::TableNextColumn();card("##prox","CAPTEUR DE PROXIMITÉ",[&]{auto raw=value("PROX","");std::optional<bool> state;if(raw.find("DISABLED")!=std::string::npos)state=false;else if(raw.find("ENABLED")!=std::string::npos)state=true;toggle("prox","Capteur de proximité",state,[&](bool on){shellAction(on?"Activer le capteur de proximité":"Désactiver le capteur de proximité",on?"am broadcast --user 0 -a com.oculus.vrpowermanager.automation_disable":"am broadcast --user 0 -a com.oculus.vrpowermanager.prox_close --ei duration 3600000");});});
                ImGui::TableNextColumn();card("##boundary","BOUNDARY / GUARDIAN",[&]{toggle("boundary","Boundary",flag("GUARDIAN",true),[&](bool on){shellAction(on?"Activer Boundary":"Désactiver Boundary pour le développement",on?"setprop debug.oculus.guardian_pause 0; am broadcast -a com.oculus.intent.action.GUARDIAN_STATE_CHANGED --ez guardian_enabled true":"setprop debug.oculus.guardian_pause 1; am broadcast -a com.oculus.intent.action.GUARDIAN_STATE_CHANGED --ez guardian_enabled false",!on);});});
                ImGui::TableNextColumn();card("##power","ALIMENTATION",[&]{{cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::button("Réveiller"))shellAction("Réveiller le casque","input keyevent 224");ImGui::SameLine();if(cyui::button("Veille"))shellAction("Mettre en veille","input keyevent 223");if(cyui::button("Redémarrer…")){confirmationText="Redémarrer le casque";confirmationJob={"Redémarrage",{{{"reboot"},{}}},30000,true};confirmDevice=true;}}});
                ImGui::TableNextColumn();card("##runtime","VR RUNTIME DEBUG",[&]{{cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::button("Lister les propriétés VR")){terminalMode=0;strncpy_s(command.data(),command.size(),"getprop | grep -E 'debug.oculus|debug.vr'",_TRUNCATE);page=4;executeTerminal();}if(cyui::button("Compteurs GPU disponibles")){terminalMode=0;strncpy_s(command.data(),command.size(),"ovrgpuprofiler -m",_TRUNCATE);page=4;executeTerminal();}}});
            }
            ImGui::EndTable();
        }
        cyui::space(10);
        if(preview->texture()) {cyui::sectionTitle("DERNIÈRE CAPTURE");auto size=ImGui::GetContentRegionAvail();float width=std::min(size.x,cyui::dp(1000));float height=width*float(preview->height())/float(preview->width());ImGui::Image(static_cast<ImTextureID>(preview->texture()),ImVec2(width,height));}
    }
    void terminal() {
        cyui::panelHeader("Commandes ADB","Exécution sur le casque sélectionné, sortie en direct et annulation par tâche.");cyui::space(10);
        const char* modes[]={"Shell Android","Commande ADB"};cyui::segmented("##terminalMode",modes,2,&terminalMode,cyui::dp(320));ImGui::SameLine();ImGui::SetNextItemWidth(cyui::dp(100));ImGui::InputInt("Délai (s)",&terminalTimeout);terminalTimeout=std::clamp(terminalTimeout,1,300);
        ImGui::SetNextItemWidth(-1);bool enter=ImGui::InputTextWithHint("##command",terminalMode?"adb shell getprop / pull / push / forward…":"getprop ro.product.model",command.data(),command.size(),ImGuiInputTextFlags_EnterReturnsTrue);
        {cyui::ScopedDisabled disabled(!online||options.demo);if(cyui::primaryButton("Exécuter")||enter)executeTerminal();}
        ImGui::SameLine();if(cyui::button("Copier la sortie")&&tasks.count(selectedTask))ImGui::SetClipboardText(tasks[selectedTask].output.c_str());
        ImGui::SameLine();if(cyui::button("Exporter…")&&tasks.count(selectedTask)){auto path=fileDialog(true,L"Texte\0*.txt\0\0",L"txt",L"adb-output.txt");if(!path.empty()){std::ofstream out(path,std::ios::binary);out<<tasks[selectedTask].output;}}
        if(tasks.count(selectedTask)){auto& t=tasks[selectedTask];if(!t.done){ImGui::SameLine();if(cyui::button("Annuler"))backend->cancel(t.id);}cyui::space(8);cyui::textDim("%s",t.label.c_str());cyui::badge(t.done?(t.ok?"TERMINÉ":"ÉCHEC / ANNULÉ"):"EN COURS",t.done?(t.ok?cyui::Tone::success:cyui::Tone::error):cyui::Tone::info);ImGui::BeginChild("##terminalOutput",ImVec2(0,0),ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);{cyui::ScopedFont font(cyui::monoFont());ImGui::InputTextMultiline("##output",t.output.data(),t.output.size()+1,ImVec2(-1,-1),ImGuiInputTextFlags_ReadOnly);}ImGui::EndChild();}
        else cyui::emptyState("Terminal prêt","Choisissez une commande rapide ou saisissez votre commande. Entrée pour exécuter.",cyui::icon::code);
    }
    void metricSelections(const Plugin& p,PluginPanel& panel){
        auto& draft=panel.draft;const bool companion=p.controller=="cyinsight_ovr";
        const char* groups[]={"Toutes","Frame / GPU","CPU","Memory","Device"};cyui::segmented("##metricGroup",groups,5,&panel.group,cyui::dp(450));
        cyui::textDim("%zu graphiques · %zu valeurs%s",draft["graphs"].size(),draft["stats"].size(),companion?" · limites : 6 / 8":"");
        if(ImGui::BeginTable("##metricSelection",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0,cyui::dp(240)))){
            ImGui::TableSetupColumn("Métrique",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Graphique",ImGuiTableColumnFlags_WidthFixed,cyui::dp(85));ImGui::TableSetupColumn("Valeur",ImGuiTableColumnFlags_WidthFixed,cyui::dp(70));ImGui::TableSetupColumn("Service",ImGuiTableColumnFlags_WidthFixed,cyui::dp(115));ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            for(const auto& m:hudMetrics()){
                if(panel.group&&std::string(m.group)!=groups[panel.group])continue;ImGui::PushID(m.id);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(m.title);cyui::tooltip(m.id);
                for(auto key:{"graphs","stats"}){
                    ImGui::TableNextColumn();auto& values=draft[key];bool chosen=std::find(values.begin(),values.end(),m.id)!=values.end();auto maximum=std::string(key)=="graphs"?6u:8u;
                    bool available=!panel.available.count(m.id)||panel.available[m.id];cyui::ScopedDisabled disabled(!chosen&&(!available||(companion&&values.size()>=maximum)));
                    if(ImGui::Checkbox((std::string("##")+key).c_str(),&chosen)){if(chosen)values.push_back(m.id);else values.erase(std::remove(values.begin(),values.end(),Json(m.id)),values.end());}
                }
                ImGui::TableNextColumn();cyui::textFaint("%s",!panel.available.count(m.id)?"À vérifier":panel.available[m.id]?"Disponible":"Indisponible");ImGui::PopID();
            }ImGui::EndTable();
        }
    }
    void configurablePluginPage(const Plugin& p,bool installed){
        auto& panel=pluginPanels[p.id];auto& draft=panel.draft;const bool companion=p.controller=="cyinsight_ovr";const bool disabled=!online||options.demo||!installed||!p.enabled||pluginBusy();
        auto run=[&](const std::string& action,bool settings=false){try{runPlugin(p,action,settings?draft:Json::object());}catch(const std::exception& e){panel.error=e.what();}};
        {cyui::ScopedDisabled scope(disabled);if(cyui::primaryButton("Appliquer au Quest"))run("apply",true);ImGui::SameLine();if(cyui::button("Lire l’état"))run("read_state");ImGui::SameLine();if(cyui::button(companion?"Arrêter / restaurer":"Masquer le HUD"))run("hide");}
        ImGui::SameLine();if(cyui::button("Enregistrer le profil"))savePluginProfile(p);
        cyui::space(6);cyui::textFaint("Les réglages ci-dessous sont un profil local. Appliquer les envoie au casque sélectionné.");
        if(panel.task){cyui::progressBar(-1,ImVec2(-cyui::dp(185),cyui::dp(4)));ImGui::SameLine();if(cyui::ghostButton("Annuler"))backend->cancel(panel.task);ImGui::SameLine();if(cyui::ghostButton("Sortie ADB")){selectedTask=panel.task;page=4;}}
        if(!panel.error.empty())cyui::callout(cyui::Tone::error,panel.error.c_str());else if(!panel.message.empty())cyui::callout(cyui::Tone::info,panel.message.c_str());
        if(!installed)cyui::callout(cyui::Tone::warning,companion?"Installez l’APK CyInsightOVR puis initialisez son collecteur dans Connexion / captures.":"Installez OVR Metrics Tool sur le Quest pour appliquer ces réglages.");
        if(ImGui::BeginTabBar("##pluginTabs")){
            if(ImGui::BeginTabItem("HUD et métriques",nullptr,pluginTabRequest&&options.pluginTab==0?ImGuiTabItemFlags_SetSelected:0)){
                cyui::space(8);bool columns=ImGui::GetContentRegionAvail().x>cyui::dp(850)&&ImGui::BeginTable("##hudColumns",2,ImGuiTableFlags_SizingStretchProp);if(columns){ImGui::TableSetupColumn("Placement",ImGuiTableColumnFlags_WidthStretch,.4f);ImGui::TableSetupColumn("Métriques",ImGuiTableColumnFlags_WidthStretch,.6f);ImGui::TableNextRow();ImGui::TableNextColumn();}
                if(cyui::beginCard("##hudPlacement","AFFICHAGE DU HUD",columns?cyui::dp(405):0)){
                    if(cyui::beginProperties("##hudSettings",cyui::dp(145))){
                        if(companion){
                            cyui::property("Taille");const char* sizes[]={"Compact","Moyen"};int size=draft["size"]=="medium"?1:0;if(cyui::combo("##hudSize",&size,sizes,2))draft["size"]=size?"medium":"small";
                            cyui::property("Position");const char* labels[]={"Haut gauche","Haut centre","Haut droite","Centre gauche","Centre","Centre droite","Bas gauche","Bas centre","Bas droite"};const char* values[]={"upper_left","upper_center","upper_right","center_left","center","center_right","lower_left","lower_center","lower_right"};int index=0;for(int i=0;i<9;++i)if(draft["corner"]==values[i])index=i;if(cyui::combo("##hudCorner",&index,labels,9))draft["corner"]=values[index];
                        }else{
                            for(const auto& pair:std::vector<std::pair<const char*,const char*>>{{"overlay","Afficher le HUD"},{"headlocked","Suivre la tête"},{"graphsVisible","Afficher les graphes"},{"statsVisible","Afficher les valeurs"},{"csv","Enregistrer en CSV"}}){cyui::property(pair.second);bool value=draft[pair.first].get<bool>();if(cyui::toggleSwitch((std::string("##")+pair.first).c_str(),&value))draft[pair.first]=value;}
                            auto number=[&](const char* label,const char* key,float low,float high,const char* format){cyui::property(label);float value=draft[key].get<float>();if(cyui::sliderBar(key,&value,low,high,format,-1))draft[key]=value;};
                            number("Inclinaison","pitch",-90,90,"%.1f°");number("Rotation","yaw",-180,180,"%.1f°");number("Échelle","scale",1,3,"%.2f");number("Distance","distance",.1f,2,"%.2f m");
                        }cyui::endProperties();
                    }
                    cyui::space(10);cyui::textFaint("%s",companion?"Moteur de rendu Meta · suivi de la tête":"Profil de configuration OVR Metrics");
                }cyui::endCard();if(columns)ImGui::TableNextColumn();else cyui::space(8);
                if(cyui::beginCard("##hudMetrics","GRAPHIQUES ET VALEURS",columns?cyui::dp(405):0)){
                    if(companion){{cyui::ScopedDisabled scope(disabled);if(cyui::button("Lire le catalogue du casque"))run("catalog");}cyui::textFaint("Le HUD utilise le moteur Meta ; l’arrêt restaure les réglages précédents.");}
                    metricSelections(p,panel);
                }cyui::endCard();if(columns)ImGui::EndTable();
                if(!companion)cyui::textFaint("Appliquer gère les 30 métriques de ce panneau. Les autres métriques sont conservées.");
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem(companion?"Connexion / captures":"Rapports CSV",nullptr,pluginTabRequest&&options.pluginTab==1?ImGuiTabItemFlags_SetSelected:0)){
                cyui::space(10);
                if(companion){
                    if(cyui::beginCard("##companionConnection","COLLECTEUR CYINSIGHTOVR")){
                        ImGui::TextWrapped("Le collecteur officiel s’exécute sur le Quest jusqu’au redémarrage. Choisissez cyinsightovr-bridge.jar dans le dossier de distribution de CyInsightOVR.");
                        {cyui::ScopedDisabled scope(disabled);if(cyui::button("Initialiser le collecteur…")){auto jar=fileDialog(false,L"Collecteur CyInsightOVR (*.jar)\0*.jar\0\0",L"jar");if(!jar.empty()){try{auto job=cyInsightBootstrap(jar);auto id=submit(job.label,job.steps,"plugin:control",{},job.timeoutMs,job.cleanupSteps);pluginRuns[id]={p,"bootstrap",Json::object()};panel.task=id;panel.error.clear();}catch(const std::exception& e){panel.error=e.what();}}}ImGui::SameLine();if(cyui::button("État du profileur"))run("status");}
                        cyui::textFaint("Un collecteur déjà actif est conservé. Après une mise à jour, utilisez -RestartBridge du lanceur CyInsightOVR.");
                    }cyui::endCard();cyui::space(8);
                    if(cyui::beginCard("##companionCapture","CAPTURE DES TUILES GPU")){
                        if(cyui::beginProperties("##captureSettings",cyui::dp(180))){
                            cyui::property("Application cible");auto target=draft["package"].get<std::string>();if(ImGui::BeginCombo("##captureTarget",target.empty()?"Choisir un package exact":target.c_str())){for(const auto& pkg:packages)if(pkg.name!=p.package){if(ImGui::Selectable(pkg.name.c_str(),target==pkg.name))draft["package"]=pkg.name;}ImGui::EndCombo();}
                            cyui::property("Durée");float seconds=draft["seconds"].get<float>();if(cyui::sliderBar("##captureSeconds",&seconds,.05f,2,"%.2f s",-1))draft["seconds"]=seconds;cyui::endProperties();
                        }
                        {cyui::ScopedDisabled scope(disabled);if(cyui::button("Activer le profilage détaillé"))run("enable_details",true);ImGui::SameLine();if(cyui::button("Capturer les tuiles GPU"))run("capture",true);}
                        ImGui::TextWrapped("Activez le profilage avant de démarrer le jeu, puis ouvrez-le manuellement. La capture ne lance ni ne redémarre le jeu. Actualisez l’état jusqu’à retrouver l’identifiant de capture demandé.");
                    }cyui::endCard();
                }else{
                    ImGui::TextWrapped("Les rapports CSV sont produits par OVR Metrics Tool dans le dossier CapturedMetrics du casque.");
                    if(cyui::primaryButton("Courbes et enregistrement sur PC")){page=8;if(online&&!options.demo)metrics->capture().request("refresh_reports");}
                    {cyui::ScopedDisabled scope(disabled);if(cyui::button("Démarrer le CSV"))run("csv_start");ImGui::SameLine();if(cyui::button("Arrêter le CSV"))run("csv_stop");}
                }
                cyui::space(12);{cyui::ScopedDisabled scope(!online&&!options.demo);if(cyui::button(companion?"Parcourir les captures .cyovr":"Parcourir les rapports CSV")){files->browse(companion?"/sdcard/Android/data/org.cyinsights.ovr/files/Captures":"/sdcard/Android/data/com.oculus.ovrmonitormetricsservice/files/CapturedMetrics");page=7;}}
                if(companion){
                    cyui::space(8);{cyui::ScopedDisabled scope(panel.capture.empty());if(cyui::button("Exporter la capture reçue…")){auto path=fileDialog(true,L"Capture CyInsightOVR (*.cyovr)\0*.cyovr\0\0",L"cyovr",L"capture.cyovr");if(!path.empty()){std::ofstream output(path,std::ios::binary);output<<panel.capture.dump();output.close();cyui::notify(output?"Capture exportée.":"Export impossible.",output?cyui::Tone::success:cyui::Tone::error);}}}
                    if(!panel.requestedCapture.empty())cyui::textMono("Demandée : %s",panel.requestedCapture.c_str());
                    if(!panel.capture.empty())cyui::textMono("Reçue : %s · %s",panel.capture.value("capture_id",std::string()).c_str(),panel.capture.value("status",std::string()).c_str());
                    cyui::textFaint("Lire l’état récupère le résultat du collecteur. L’export .cyovr s’ouvre dans CyInsights sur PC.");
                }
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("État du service",nullptr,pluginTabRequest&&options.pluginTab==2?ImGuiTabItemFlags_SetSelected:0)){
                cyui::space(10);cyui::textDim("Dernière réponse du casque sélectionné");cyui::textFaint("La validation des réglages ne confirme pas leur visibilité physique dans le casque.");
                if(panel.observed.empty())cyui::emptyState("État pas encore lu","Utilisez Lire l’état pour interroger le service du Quest.",cyui::icon::info);
                else{
                    if(!companion&&panel.observed.contains("configuration")){if(cyui::button("Copier l’état dans le profil")){try{panel.draft=validatePluginSettings(p,panel.observed.at("configuration"));}catch(const std::exception& e){panel.error=e.what();}}}
                    auto text=panel.observed.dump(2);ImGui::InputTextMultiline("##pluginState",text.data(),text.size()+1,ImVec2(-1,cyui::dp(380)),ImGuiInputTextFlags_ReadOnly);
                }ImGui::EndTabItem();
            }ImGui::EndTabBar();pluginTabRequest=false;
        }
        cyui::space(8);{cyui::ScopedDisabled scope(!online||options.demo||pluginBusy());if(cyui::ghostButton("Installer un APK…"))install();}ImGui::SameLine();if(cyui::ghostButton("Ouvrir ses logs")){openApp(p.package);page=0;}
    }
    void pluginPage() {
        if(plugins.empty()){cyui::emptyState("Aucun plugin","Créez un plugin ou ajoutez un fichier .cyplugin dans le dossier des plugins.",cyui::icon::library);return;}
        auto& p=plugins[size_t(std::clamp(selectedPlugin,0,int(plugins.size())-1))];
        auto installed=std::any_of(packages.begin(),packages.end(),[&](const Package& a){return a.name==p.package;});
        if(configurablePlugin(p)&&p.error.empty()){
            cyui::panelHeader(p.name.c_str(),p.description.c_str());cyui::space(8);ImGui::Checkbox("Plugin activé",&p.enabled);ImGui::SameLine();cyui::badge(installed?"APP DÉTECTÉE":"APP ABSENTE",installed?cyui::Tone::success:cyui::Tone::warning);ImGui::SameLine();cyui::textMono("%s",p.package.c_str());cyui::space(8);configurablePluginPage(p,installed);return;
        }
        cyui::panelHeader("Plugins et applications liées","Extensions rechargeables, actions ADB et ponts TCP vers les services de vos apps Quest.");cyui::space(10);
        cyui::heading(p.name.c_str(),1);cyui::textDim("%s · %s",p.id.c_str(),p.version.c_str());ImGui::TextWrapped("%s",p.description.c_str());cyui::space(12);
        if(!p.error.empty()){cyui::callout(cyui::Tone::error,p.error.c_str());return;}
        ImGui::Checkbox("Plugin activé",&p.enabled);cyui::badge(installed?"APP DÉTECTÉE":"APP ABSENTE",installed?cyui::Tone::success:cyui::Tone::warning);ImGui::SameLine();cyui::textMono("%s",p.package.c_str());
        cyui::space(8);if(cyui::beginCard("##pluginBridge","LIAISON AVEC LE CASQUE")) {
            if(p.localPort&&p.remotePort){cyui::textMono("%s · localhost:%d ↔ Quest:%d",p.reverse?"ADB reverse":"ADB forward",p.localPort,p.remotePort);cyui::textDim("%s",pluginLinked[p.id]?"Tunnel ADB actif":"Tunnel non établi dans cette session");cyui::textFaint("L’application cible doit fournir le service TCP correspondant.");{cyui::ScopedDisabled disabled(!online||options.demo||!p.enabled||!installed);if(cyui::button("Relier l’application")){auto request=pluginLink(p);submit(request.label,request.steps,"link:"+p.id);}ImGui::SameLine();if(cyui::button("Déconnecter")){auto request=pluginLink(p,true);submit(request.label,request.steps,"unlink:"+p.id);}}}
            else cyui::textDim("Ce plugin déclare des actions sans service TCP.");
            {cyui::ScopedDisabled disabled(!online||options.demo||!installed||!p.enabled);if(cyui::button("Lancer l’app"))backend->action("launch",p.package);}ImGui::SameLine();if(cyui::button("Ouvrir ses logs")){openApp(p.package);page=0;}
        }cyui::endCard();cyui::space(10);
        if(cyui::beginCard("##pluginActions","ACTIONS DU PLUGIN")){for(const auto& a:p.actions){ImGui::PushID(a.id.c_str());{cyui::ScopedDisabled disabled(!online||options.demo||!p.enabled||!installed);if(cyui::button(a.label.c_str())){auto request=pluginAction(p,a);submit(request.label,request.steps,"plugin",{},request.timeoutMs);}}cyui::tooltip(a.command.c_str());ImGui::PopID();}}cyui::endCard();cyui::space(10);cyui::textFaint("Manifest : %s",narrow(p.path.wstring()).c_str());
    }
    void filesPage() { files->draw(); }
    void assistantPage() {
        cyui::panelHeader("Discussion IA","Assistant contextualisé et outils QuestDev via CyToolsCore.");cyui::space(8);
        cyui::badge(assistantReady?"MCP / SDK ACTIF":options.demo?"DÉMONSTRATION":"RUNTIME HORS LIGNE",assistantReady?cyui::Tone::success:cyui::Tone::neutral);ImGui::SameLine();cyui::textDim("%s",assistantStatus.c_str());
        if(!assistantReady){
            {cyui::ScopedDisabled disabled(options.demo||assistant->running());if(cyui::primaryButton("Démarrer MCP / SDK"))startAssistant();}
            auto script=productRoot()/L"runtime/setup.ps1";if(isFile(script.wstring())){ImGui::SameLine();if(cyui::button("Installer / réparer le runtime")){auto args=L"-NoProfile -ExecutionPolicy Bypass -File \""+script.wstring()+L"\"";ShellExecuteW(nullptr,L"open",L"powershell.exe",args.c_str(),productRoot().c_str(),SW_SHOWNORMAL);}}
        }
        if(ImGui::CollapsingHeader("Connexion IA et modèle",assistantConnected?0:ImGuiTreeNodeFlags_DefaultOpen)){
            const char* names[]={"Codex · compte CLI","Claude Code · compte CLI","Ollama · serveur local","OpenRouter · API","Claude · API","OpenCode · CLI","Antigravity · CLI"};
            if(cyui::beginProperties("##aiConnection",cyui::dp(145))){
                cyui::property("Fournisseur");if(cyui::combo("##provider",&provider,names,7))aiEndpoint.fill(0);
                cyui::property("Modèle");ImGui::InputTextWithHint("##model","Défaut du fournisseur / ID exact",aiModel.data(),aiModel.size());
                if(!aiModels.empty()){cyui::property("Catalogue");if(ImGui::BeginCombo("##models","Choisir un modèle découvert")){for(const auto& m:aiModels){auto model=m.is_string()?m.get<std::string>():m.value("model",m.value("id",std::string()));if(!model.empty()&&ImGui::Selectable(model.c_str()))strncpy_s(aiModel.data(),aiModel.size(),model.c_str(),_TRUNCATE);}ImGui::EndCombo();}}
                if(provider>=2&&provider<=4){cyui::property("URL du serveur");ImGui::InputTextWithHint("##endpoint",provider==2?"http://127.0.0.1:11434":provider==3?"https://openrouter.ai/api/v1":"https://api.anthropic.com/v1",aiEndpoint.data(),aiEndpoint.size());cyui::property("Variable de clé API");ImGui::InputTextWithHint("##keyEnv","Nom de variable (facultatif)",aiKeyEnv.data(),aiKeyEnv.size());cyui::property("Clé API · session");ImGui::InputTextWithHint("##sessionApiKey","Non enregistrée sur disque",aiKey.data(),aiKey.size(),ImGuiInputTextFlags_Password);}
                else {cyui::property("Exécutable CLI");ImGui::InputTextWithHint("##aiExecutable","Détection automatique",aiExecutable.data(),aiExecutable.size());}
                cyui::endProperties();
            }
            {cyui::ScopedDisabled disabled(!assistantReady||assistantBusy||options.demo);if(cyui::button("Connecter / appliquer le profil")){
                const char* ids[]={"codex","claude-code","ollama","openrouter","anthropic","opencode","antigravity"};Json config={{"connector",ids[provider]},{"model",aiModel.data()},{"endpoint",aiEndpoint.data()},{"executable",aiExecutable.data()},{"api_key_env",aiKeyEnv.data()}};
                assistant->send({{"type","connect"},{"config",config},{"apiKey",aiKey.data()}});SecureZeroMemory(aiKey.data(),aiKey.size());assistantConnected=false;assistantBusy=true;
                Json profile={{"provider",provider},{"model",aiModel.data()},{"endpoint",aiEndpoint.data()},{"executable",aiExecutable.data()},{"api_key_env",aiKeyEnv.data()}};std::ofstream out(userRoot()/L"ai-profile.json");out<<profile.dump(2);chat.push_back({"Système","Connexion au fournisseur sélectionné…"});
            }}
            cyui::textFaint("Codex : outils QuestDev natifs. Autres fournisseurs : discussion avec contexte ; contrôle externe par MCP.");
        }
        cyui::space(8);
        float historyHeight=std::max(cyui::dp(100),ImGui::GetContentRegionAvail().y-cyui::dp(140));
        ImGui::BeginChild("##chatHistory",ImVec2(0,historyHeight),ImGuiChildFlags_Borders);
        if(chat.empty()){cyui::space(20);cyui::heading("Travailler avec le contexte du Quest",2);ImGui::TextWrapped("Connectez une IA puis demandez, par exemple : analyse les erreurs de l’application sélectionnée. Les logs et les résultats ADB sont traités comme des données, pas comme des instructions.");}
        for(size_t i=0;i<chat.size();++i){ImGui::PushID(int(i));cyui::overline(chat[i].first.c_str());ImGui::TextWrapped("%s",chat[i].second.c_str());if(cyui::ghostButton("Copier"))ImGui::SetClipboardText(chat[i].second.c_str());cyui::divider();ImGui::PopID();}
        if(assistantBusy)ImGui::SetScrollY(ImGui::GetScrollMaxY());ImGui::EndChild();
        ImGui::InputTextMultiline("##prompt",aiPrompt.data(),aiPrompt.size(),ImVec2(-1,cyui::dp(80)));
        {cyui::ScopedDisabled disabled(!assistantReady||!assistantConnected||assistantBusy||options.demo);if(cyui::primaryButton("Envoyer") || (ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Enter)&&!assistantBusy&&assistantConnected)){
            auto text=trim(aiPrompt.data());if(!text.empty()){assistant->send({{"type","context"},{"context",contextSnapshot()}});assistant->send({{"type","chat"},{"message",text},{"includeContext",sendContext},{"includeLogs",sendLogs}});chat.push_back({"Vous",text});aiPrompt.fill(0);assistantBusy=true;}
        }}
        ImGui::SameLine();if(assistantBusy&&cyui::button("Arrêter"))assistant->send({{"type","cancel"}});
        ImGui::SameLine();if(cyui::button("Exporter la discussion…")){auto path=fileDialog(true,L"Texte\0*.txt\0\0",L"txt",L"questdev-conversation.txt");if(!path.empty()){std::ofstream out(path,std::ios::binary);for(const auto& line:chat)out<<line.first<<"\n"<<line.second<<"\n\n";}}
    }
    void launchUpdate(const Json& value) {
        auto plan=std::filesystem::path(widen(value.at("plan").get<std::string>()));auto python=std::filesystem::path(widen(value.at("python").get<std::string>()));auto script=std::filesystem::path(widen(value.at("script").get<std::string>()));auto ready=std::filesystem::path(widen(value.at("ready").get<std::string>()));
        auto base=std::filesystem::weakly_canonical(userRoot()/L"updates/jobs");auto relative=plan.lexically_relative(base);
        if(!plan.is_absolute()||relative.empty()||*relative.begin()==L".."||plan.filename()!=L"plan.json"||python!=plan.parent_path()/L"bootstrap/python.exe"||script!=plan.parent_path()/L"bootstrap/apply.py"||ready!=plan.parent_path()/L"ready.json")throw std::runtime_error("Préparation de mise à jour invalide.");
        auto quote=[](const std::wstring& text){std::wstring out=L"\"";size_t n=0;for(auto c:text){if(c==L'\\'){++n;continue;}out.append(c==L'"'?2*n+1:n,L'\\');out+=c;n=0;}out.append(n*2,L'\\');return out+L'"';};
        auto updateCommand=quote(python.wstring())+L" -I -S "+quote(script.wstring())+L" --plan "+quote(plan.wstring());STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        if(!CreateProcessW(python.c_str(),updateCommand.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,python.parent_path().c_str(),&startup,&process))throw std::runtime_error("L’installateur n’a pas pu démarrer.");
        CloseHandle(process.hThread);updateInstaller=process.hProcess;updateReady=ready;updateLaunchTime=std::chrono::steady_clock::now();
    }
    void tickUpdates() {
        if(updateBridge)for(auto& event:updateBridge->poll())if(event.value("type",std::string())=="update_state"){
            event.erase("type");updateState.update(event);
            if(event.contains("current")&&event.contains("prefs"))updateWorkerReady=true;
            if(event.value("phase",std::string())=="available"&&event.contains("release")&&!event["release"].is_null()){auto text="Version "+event["release"].value("version",std::string())+" disponible · Réglages > Mises à jour GitHub";cyui::notify(text.c_str(),cyui::Tone::info,6);}
            if(event.contains("prepared")){try{launchUpdate(event.at("prepared"));}catch(const std::exception& e){updateState["error"]=e.what();updateState["busy"]=false;}updateState.erase("prepared");}
            if(!updateAutoChecked&&event.contains("prefs")){updateAutoChecked=true;if(event.at("prefs").value("autoCheck",false))updateBridge->send({{"type","check"}});}
        }
        if(updateInstaller){
            bool finished=false,accepted=false;
            try{if(std::filesystem::is_regular_file(updateReady)&&std::filesystem::file_size(updateReady)<4096){std::ifstream input(updateReady);Json result;input>>result;if(result.value("ok",false)){accepted=true;PostMessageW(static_cast<HWND>(options.windowHandle),WM_CLOSE,0,0);}else{updateState["error"]=result.value("error",std::string("L’installation a été annulée."));updateState["busy"]=false;}finished=true;}}
            catch(...){updateState["error"]="Réponse de l’installateur invalide.";updateState["busy"]=false;finished=true;}
            if(!finished&&(WaitForSingleObject(updateInstaller,0)==WAIT_OBJECT_0||std::chrono::steady_clock::now()-updateLaunchTime>std::chrono::seconds(15))){updateState["error"]="L’installateur n’a pas confirmé son démarrage. L’application reste ouverte.";updateState["busy"]=false;finished=true;}
            if(finished){if(!accepted&&WaitForSingleObject(updateInstaller,0)==WAIT_TIMEOUT){TerminateProcess(updateInstaller,1);WaitForSingleObject(updateInstaller,2000);}CloseHandle(updateInstaller);updateInstaller=nullptr;}
        }
    }
    void updateAction(const char* kind) {
        if(!updateBridge||!updateBridge->running())return;
        Json message={{"type",kind}};
        if(std::string(kind)=="install"||std::string(kind)=="rollback"){if(files->busy()){cyui::notify("Terminez ou annulez la liste de transferts avant la mise à jour.",cyui::Tone::warning);return;}FILETIME created{},exited{},kernel{},user{};GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);message["target"]=appRoot().u8string();message["parentPid"]=GetCurrentProcessId();message["parentCreated"]=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;}
        updateBridge->send(message);if(std::string(kind)!="cancel")updateState["busy"]=true;
    }
    void updatesPage() {
        cyui::panelHeader("Mises à jour GitHub","Téléchargement vérifié, sauvegarde de la version actuelle et redémarrage.");cyui::space(10);
        bool busy=updateState.value("busy",false),available=updateState.contains("release")&&!updateState["release"].is_null(),downloaded=updateState.value("downloaded",false);auto prefs=updateState.value("prefs",Json::object());
        if(cyui::beginCard("##updates","CYVR QUESTDEV")){
            cyui::heading((std::string("Version ")+AppVersion).c_str(),2);cyui::textDim("Source : MrMybal / CyVR_QuestDev · Windows x64");
            {cyui::ScopedDisabled disabled(busy||options.demo||!updateBridge||!updateBridge->running());
                int channel=prefs.value("channel",std::string("alpha"))=="stable"?0:1;bool automatic=prefs.value("autoCheck",false),git=prefs.value("useGit",true);bool changed=false;
                if(cyui::beginProperties("##updatePrefs",cyui::dp(210))){const char* channels[]={"Versions stables","Alpha et préversions"};cyui::property("Canal de mise à jour");if(cyui::combo("##updateChannel",&channel,channels,2,-1))changed=true;
                    cyui::property("Rechercher au démarrage");if(cyui::toggleSwitch("##updateAutomatic",&automatic))changed=true;
                    cyui::property("Connexion Git existante");if(cyui::toggleSwitch("##updateGit",&git))changed=true;cyui::endProperties();}
                if(changed)updateBridge->send({{"type","preferences"},{"values",{{"channel",channel?"alpha":"stable"},{"autoCheck",automatic},{"useGit",git}}}});
                if(cyui::button("Rechercher une mise à jour"))updateAction("check");
            }
            cyui::space(8);ImGui::TextWrapped("%s",updateState.value("status",std::string()).c_str());
            if(busy){auto phase=updateState.value("phase",std::string());cyui::progressBar(phase=="downloading"?float(updateState.value("progress",0.0)):-1.0f,ImVec2(-1,0));if(phase=="checking"||phase=="downloading")if(cyui::ghostButton("Annuler"))updateAction("cancel");}
            if(available){auto release=updateState["release"];cyui::space(8);cyui::heading(("Disponible : "+release.value("version",std::string())).c_str(),3);cyui::textDim("%.1f Mo",release.value("size",0.0)/(1024*1024));
                {cyui::ScopedDisabled disabled(busy||options.demo);if(!downloaded){if(cyui::primaryButton("Télécharger et vérifier"))updateAction("download");}else{bool recording=videoMirror->state().active||recordTask||files->busy()||metrics->describe().value("recording",false);bool portable=isFile((appRoot()/L"RELEASE.json").wstring());{cyui::ScopedDisabled blocked(recording||!portable);if(cyui::primaryButton("Installer et redémarrer"))updateAction("install");}if(recording)cyui::textFaint("Terminez les transferts et arrêtez les enregistrements / mirroring avant l’installation.");if(!portable)cyui::textFaint("Build de développement : installez une archive portable pour la mise à jour intégrée.");}}
                if(ImGui::CollapsingHeader("Notes de version")){ImGui::PushTextWrapPos();ImGui::TextUnformatted(release.value("notes",std::string()).c_str());ImGui::PopTextWrapPos();}
            }
            if(!updateState.value("error",std::string()).empty())cyui::callout(cyui::Tone::error,updateState["error"].get<std::string>().c_str());
        }cyui::endCard();cyui::space(10);
        if(cyui::beginCard("##updateAuth","ACCÈS AU DÉPÔT PRIVÉ")){
            ImGui::TextWrapped("Le compte GitHub doit avoir accès au dépôt. Utilisez la connexion Git existante ou un jeton à accès fin limité à ce dépôt, avec Contents en lecture.");
            cyui::textFaint("%s",updateState.value("savedCredential",false)?"Jeton conservé dans le Gestionnaire d’identifiants Windows.":"Aucun jeton spécifique enregistré dans Windows.");
            {cyui::ScopedDisabled disabled(busy||options.demo||!updateBridge||!updateBridge->running());ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##updateToken","Jeton GitHub (masqué)",updateToken.data(),updateToken.size(),ImGuiInputTextFlags_Password);
                if(cyui::button("Enregistrer dans Windows")){updateBridge->send({{"type","save_token"},{"token",updateToken.data()}});SecureZeroMemory(updateToken.data(),updateToken.size());}ImGui::SameLine();if(cyui::ghostButton("Oublier ce jeton"))updateBridge->send({{"type","forget_token"}});}
            if(cyui::ghostButton("Créer un jeton sur GitHub"))ShellExecuteW(nullptr,L"open",L"https://github.com/settings/personal-access-tokens/new",nullptr,nullptr,SW_SHOWNORMAL);
        }cyui::endCard();cyui::space(10);
        if(cyui::beginCard("##updateBackup","VERSION PRÉCÉDENTE")){
            if(files->busy())cyui::callout(cyui::Tone::warning,"Terminez ou annulez votre liste de transferts avant de redémarrer pour une mise à jour.");
            ImGui::TextWrapped("La version actuelle est sauvegardée avant le remplacement. Les préférences utilisateur et les plugins personnels sont conservés. Si le nouveau démarrage échoue, la sauvegarde est restaurée.");
            auto previous=updateState.value("previousVersion",std::string());if(!previous.empty()){cyui::textDim("Sauvegarde : %s",previous.c_str());{cyui::ScopedDisabled disabled(busy||options.demo||videoMirror->state().active||recordTask||files->busy()||metrics->describe().value("recording",false));if(cyui::button("Restaurer et redémarrer"))updateAction("rollback");}}
            if(cyui::ghostButton("Ouvrir le dossier des mises à jour")){auto path=userRoot()/L"updates";std::filesystem::create_directories(path);openLocal(path);}
        }cyui::endCard();
    }
    void logSettingsCard(){
        if(cyui::beginCard("##retention","LIMITES DU JOURNAL")){
            ImGui::TextWrapped("La capture conserve les messages les plus récents. Augmentez les limites pour garder davantage d’historique ; la première limite atteinte déclenche la rotation.");cyui::space(8);
            if(cyui::beginProperties("##retentionProperties",cyui::dp(190))){
                cyui::property("Nombre de lignes","De 1 000 à 5 000 000 lignes, toutes applications confondues.");ImGui::InputInt("##logLines",&logLines,10000,100000);if(ImGui::IsItemDeactivatedAfterEdit())logLines=std::clamp(logLines,1000,5000000);
                cyui::property("Mémoire (Mo)","De 16 à 2 048 Mo de données comptabilisées dans le journal. La mémoire totale du processus peut être supérieure.");ImGui::InputInt("##logMemory",&logMemoryMb,64,256);if(ImGui::IsItemDeactivatedAfterEdit())logMemoryMb=std::clamp(logMemoryMb,16,2048);
                cyui::property("Préréglages");if(cyui::button("Standard")){logLines=50000;logMemoryMb=64;}ImGui::SameLine();if(cyui::button("Étendu")){logLines=250000;logMemoryMb=256;}ImGui::SameLine();if(cyui::button("Longue session")){logLines=1000000;logMemoryMb=1024;}
                cyui::endProperties();
            }
            {cyui::ScopedDisabled disabled(store.capacity()==size_t(logLines)&&store.byteLimit()==size_t(logMemoryMb)*1024*1024);if(cyui::primaryButton("Appliquer les limites")){logLines=std::clamp(logLines,1000,5000000);logMemoryMb=std::clamp(logMemoryMb,16,2048);store.setLimits(size_t(logLines),size_t(logMemoryMb)*1024*1024);saveSettings();cyui::notify("Limites du journal appliquées et enregistrées.",cyui::Tone::success);}}
            cyui::space(6);cyui::textDim("Limites actives : %zu lignes / %zu Mo · %zu lignes conservées",store.capacity(),store.byteLimit()/(1024*1024),store.entries().size());
            cyui::textFaint("Réduire les limites retire immédiatement les messages les plus anciens en excès.\nLa pause suspend l’affichage ; la capture continue. L’export contient la vue filtrée.");
        }cyui::endCard();
    }
    void settings() {
        if(settingsSection==1){updatesPage();return;}
        if(settingsSection==2){cyui::panelHeader("Limites du journal","Choisissez l’historique conservé pendant vos sessions de développement.");cyui::space(12);logSettingsCard();return;}
        cyui::panelHeader("Réglages", "Connexion, apparence et fonctionnement du journal.");cyui::space(12);
        if(cyui::beginCard("##license","CYVR QUESTDEV · GPLv3")) {
            cyui::textDim("Copyright © 2026 Cyberalien · GPL-3.0-only");
            ImGui::TextWrapped("Logiciel libre : vous pouvez le redistribuer et le modifier selon la GNU GPL version 3. Fourni sans aucune garantie. Les composants tiers conservent leurs propres licences.");
            if(cyui::button("Lire la licence GPLv3"))openLocal(appRoot()/L"LICENSE.txt");
            ImGui::SameLine();if(cyui::button("Mises à jour GitHub…"))settingsSection=1;
        }cyui::endCard();cyui::space(10);
        if(cyui::beginCard("##adb","ANDROID DEBUG BRIDGE")) {
            if(cyui::beginProperties("##adbProps",cyui::dp(140))) {
                cyui::property("Exécutable ADB");ImGui::InputText("##adbPath",adbField.data(),adbField.size());
                cyui::property("Détection");if(cyui::ghostButton("Rechercher adb.exe")){auto p=findAdb();strncpy_s(adbField.data(),adbField.size(),narrow(p).c_str(),_TRUNCATE);}
                ImGui::SameLine();if(cyui::ghostButton("Parcourir…")){auto p=fileDialog(false,L"Android Debug Bridge (adb.exe)\0adb.exe\0\0",L"exe");if(!p.empty())strncpy_s(adbField.data(),adbField.size(),narrow(p).c_str(),_TRUNCATE);}
                cyui::property("Appliquer");{cyui::ScopedDisabled disabled(options.demo);if(cyui::button("Utiliser cet ADB")){auto p=widen(adbField.data());if(isFile(p)){adb=p;backend->setAdb(adb);serial.clear();files->tick("",session+1,false,false);store.reset();packages.clear();saveSettings();cyui::notify("ADB configuré",cyui::Tone::success);}else cyui::notify("Le fichier ADB n’existe pas.",cyui::Tone::error);}}
                cyui::endProperties();
            }
            cyui::textFaint("Détection : dossier de l’EXE, SDK Android, puis PATH.");
        }cyui::endCard();cyui::space(10);
        if(cyui::beginCard("##appearance","APPARENCE")) {
            if(cyui::beginProperties("##appearanceProps",cyui::dp(140))) {
                cyui::property("Thème");if(ImGui::BeginCombo("##theme",cyui::currentThemeKey().c_str())){for(const auto& t:cyui::builtInThemes())if(ImGui::Selectable(t.name.c_str(),t.key==cyui::currentThemeKey())){cyui::setTheme(t.key.c_str());saveSettings();}ImGui::EndCombo();}
                cyui::property("Densité");const char* names[]={"Compacte","Confortable"};int d=cyui::density()==cyui::Density::compact?0:1;if(cyui::combo("##density",&d,names,2))cyui::setDensity(d?cyui::Density::comfortable:cyui::Density::compact);
                cyui::endProperties();
            }
        }cyui::endCard();cyui::space(10);
        if(cyui::beginCard("##help","CONNEXION AU QUEST")) {
            ImGui::TextWrapped("1. Activez le mode développeur du casque.\n2. Branchez le Quest en USB et acceptez le débogage USB dans le casque.\n3. Sélectionnez le casque en haut de la fenêtre.");
            cyui::space(8);cyui::textDim("Wi-Fi : connexion à un casque dont ADB TCP/IP est déjà activé.");
            cyui::textField("##network",network.data(),int(network.size()),"192.168.1.42:5555",cyui::dp(250));ImGui::SameLine();
            {cyui::ScopedDisabled disabled(options.demo);if(cyui::button("Connecter"))backend->action("connect",network.data());}
            cyui::space(8);cyui::textFaint("Unauthorized : acceptez l’autorisation dans le casque. Offline : rebranchez puis actualisez.");
        }cyui::endCard();cyui::space(10);
        logSettingsCard();
        cyui::textFaint("Les origines inconnues restent dans Système / non attribué. Aucun rattachement approximatif par UID partagé.");
    }
    void modals() {
        if(cyui::beginModal("Mirroring vidéo",&mirrorSetup,720)){
            auto video=videoMirror->state();cyui::heading("Mirroring du Quest",2);cyui::textDim("%s",video.active?video.status.c_str():"Flux vidéo dans une fenêtre dédiée · F11 pour le plein écran");cyui::space(8);
            {cyui::ScopedDisabled disabled(video.active);
                if(cyui::beginProperties("##mirrorProperties",cyui::dp(180))){
                    cyui::property("Résolution maximale");ImGui::SliderInt("##mirrorSize",&mirrorDraft.maxSize,256,4096,"%d px");
                    cyui::property("Fréquence maximale");ImGui::SliderInt("##mirrorFps",&mirrorDraft.maxFps,15,120,"%d FPS");
                    cyui::property("Débit vidéo");ImGui::SliderInt("##mirrorRate",&mirrorDraft.bitRateMbps,1,100,"%d Mbit/s");
                    cyui::property("Audio PC");const char* audio[]={"Désactivé","Dupliquer · garder le son du casque","PC seulement · coupe le son du casque"};cyui::combo("##mirrorAudio",&mirrorDraft.audio,audio,3,-1);
                    cyui::property("Plein écran au départ");cyui::toggleSwitch("##mirrorFull",&mirrorDraft.fullscreen);
                    cyui::property("Toujours au premier plan");cyui::toggleSwitch("##mirrorTop",&mirrorDraft.alwaysOnTop);
                    cyui::property("Recadrage personnalisé");cyui::toggleSwitch("##mirrorCrop",&mirrorCrop);
                    if(mirrorCrop){cyui::property("Largeur / hauteur");ImGui::SetNextItemWidth(cyui::dp(125));ImGui::InputInt("##cropW",&mirrorDraft.cropWidth);ImGui::SameLine();ImGui::SetNextItemWidth(cyui::dp(125));ImGui::InputInt("##cropH",&mirrorDraft.cropHeight);cyui::property("Décalage X / Y");ImGui::SetNextItemWidth(cyui::dp(125));ImGui::InputInt("##cropX",&mirrorDraft.cropX);ImGui::SameLine();ImGui::SetNextItemWidth(cyui::dp(125));ImGui::InputInt("##cropY",&mirrorDraft.cropY);}
                    cyui::property("Enregistrement du flux");if(cyui::button("Choisir un fichier MKV…")){auto file=fileDialog(true,L"Vidéo Matroska (*.mkv)\0*.mkv\0\0",L"mkv",L"quest-mirroring.mkv");if(!file.empty()){mirrorDraft.recording=file;mirrorDraft.overwriteRecording=true;}}if(!mirrorDraft.recording.empty()){cyui::textDim("%s",mirrorDraft.recording.filename().u8string().c_str());if(cyui::ghostButton("Sans enregistrement"))mirrorDraft.recording.clear();}
                    cyui::endProperties();
                }
            }
            cyui::textFaint("La duplication audio nécessite Android 13+ et peut être refusée par une application.\nLe contrôle clavier/souris du casque est désactivé. Le débit réel dépend du casque et de la liaison ADB.");
            if(video.active){cyui::space(6);cyui::textMono("%d × %d · %d FPS · audio : %s",video.width,video.height,video.fps,(video.audioStatus=="off"?"désactivé":video.audioStatus=="unavailable"?"indisponible":video.audioStatus=="requested_duplicate"?"duplication demandée":"sortie PC demandée"));}
            if(!video.recordingFile.empty()){cyui::textDim("%s : %s",video.recording?"Enregistrement":"Dernier fichier",video.recordingFile.filename().u8string().c_str());if(!video.active&&cyui::ghostButton("Ouvrir le dossier du fichier"))openLocal(video.recordingFile.parent_path());}
            if(!video.error.empty())cyui::callout(cyui::Tone::error,video.error.c_str());
            if(ImGui::CollapsingHeader("Journal du mirroring")){ImGui::BeginChild("##mirrorLog",cyui::dp(0,120),ImGuiChildFlags_Borders);ImGui::TextUnformatted(video.log.c_str());ImGui::EndChild();if(cyui::ghostButton("Copier le journal"))ImGui::SetClipboardText(video.log.c_str());}
            cyui::space(10);{cyui::ScopedDisabled disabled(video.active||!online||options.demo);if(cyui::primaryButton("Démarrer la diffusion")){try{auto config=mirrorDraft;if(!mirrorCrop)config.cropWidth=config.cropHeight=config.cropX=config.cropY=0;mirrorUi.push_back(videoMirror->start(mirrorEngine,adb,serial,config));mirrorDraft.recording.clear();mirrorDraft.overwriteRecording=false;mirrorSetup=false;ImGui::CloseCurrentPopup();}catch(const std::exception& error){cyui::notify(error.what(),cyui::Tone::error);}}}ImGui::SameLine();if(cyui::button("Fermer")){mirrorSetup=false;ImGui::CloseCurrentPopup();}cyui::endModal();
        }
        bool ask=!approvals.empty(),approvalHandled=false;
        if(cyui::beginModal("Outil IA · autorisation",&ask,700)){
            if(approvals.empty())ImGui::CloseCurrentPopup();
            if(!approvals.empty()){const auto& request=approvals.front();cyui::heading(request.value("operation",std::string()).c_str(),2);auto params=request.value("parameters",Json::object()).dump(2);ImGui::TextWrapped("%s",params.c_str());cyui::textFaint("Casque cible : %s",serial.c_str());auto choice=cyui::modalButtons("Autoriser cette action","Refuser");if(choice){approvalHandled=true;auto next=approvals.front();approvals.erase(approvals.begin());if(choice==1)executeNative(next);else replyNative(next.value("id",std::string()),false,Json::object(),"Action refusée par l’utilisateur.");}}
            cyui::endModal();
        }
        if(!ask&&!approvalHandled&&!approvals.empty()){auto request=approvals.front();approvals.erase(approvals.begin());replyNative(request.value("id",std::string()),false,Json::object(),"Demande fermée par l’utilisateur.");}
        if(cyui::beginModal("Action sur le casque",&confirmDevice)) {
            ImGui::TextWrapped("%s ?",confirmationText.c_str());
            auto choice=cyui::modalButtons("Exécuter","Annuler",true);if(choice==1)submit(confirmationJob.label,confirmationJob.steps,"device:control");else if(choice==2)deviceRequests.erase("boundary");
            cyui::endModal();
        }
        if(cyui::beginModal("Créer un plugin",&newPlugin,620)) {
            if(cyui::beginProperties("##newPlugin")){
                cyui::property("Nom");ImGui::InputText("##pluginName",pluginName.data(),pluginName.size());
                cyui::property("Package Android");ImGui::InputText("##pluginPackage",pluginPackage.data(),pluginPackage.size());
                cyui::property("Port local PC");ImGui::InputInt("##localPort",&pluginLocalPort);
                cyui::property("Port du service Quest");ImGui::InputInt("##remotePort",&pluginRemotePort);
                cyui::endProperties();
            }
            cyui::textFaint("Le service doit exister dans l’app cible. Les ports ne créent pas ce service.");
            if(cyui::modalButtons("Créer","Annuler")==1){
                auto package=trim(pluginPackage.data());if(validPackage(package)&&pluginName[0]&&pluginLocalPort>0&&pluginLocalPort<=65535&&pluginRemotePort>0&&pluginRemotePort<=65535){
                    auto directory=userRoot()/L"plugins";std::filesystem::create_directories(directory);auto path=directory/widen(package+".cyplugin");
                    if(isFile(path.wstring())){newPlugin=true;cyui::notify("Un plugin existe déjà pour ce package. Modifiez son fichier dans le dossier utilisateur.",cyui::Tone::warning);}
                    else{std::ofstream out(path);out<<"[plugin]\napi=1\nid="<<package<<"\nname="<<pluginName.data()<<"\nversion=1.0\npackage="<<package<<"\n[link]\nlocal_port="<<pluginLocalPort<<"\nremote_port="<<pluginRemotePort<<"\ndirection=forward\n[action.status]\nlabel=État de l’application\nshell=dumpsys package {package}\n";out.close();if(!out)cyui::notify("Écriture du plugin impossible.",cyui::Tone::error);reloadPlugins();}
                }else{newPlugin=true;cyui::notify("Nom, package et ports valides requis.",cyui::Tone::error);}
            }cyui::endModal();
        }
        if(cyui::beginModal("Forcer l’arrêt",&confirmStop)) {
            ImGui::TextWrapped("Arrêter %s sur le casque ?",pendingStop.c_str());
            if(cyui::modalButtons("Arrêter","Annuler",true)==1 && backend)backend->action("stop",pendingStop);
            cyui::endModal();
        }
        if(cyui::beginModal("Inspecteur de message",&detailsOpen,cyui::dp(800))) {
            if(detail) {
                cyui::textDim("%s · %c · PID %d / TID %d",clockText(detail->epoch).c_str(),detail->level,detail->pid,detail->tid);
                cyui::textMono("%s",detail->package.empty()?"Système / non attribué":detail->package.c_str());
                cyui::textFaint("Processus : %s",detail->process.empty()?"inconnu":detail->process.c_str());
                cyui::sectionTitle(detail->tag.c_str());
                ImGui::BeginChild("##message",ImVec2(0,cyui::dp(280)),ImGuiChildFlags_Borders);
                ImGui::TextWrapped("%s",detail->message.c_str());ImGui::EndChild();
                if(cyui::button("Copier le message"))ImGui::SetClipboardText(formatLog(*detail).c_str());
            } else cyui::textDim("Sélectionnez une ligne dans la console.");
            cyui::endModal();
        }
    }
    void frame() {
        tick();rebuildCounts();
        if(!ImGui::GetIO().WantTextInput&&!ImGui::IsAnyItemActive()) {
            if(ImGui::IsKeyPressed(ImGuiKey_F5)&&backend)backend->refresh();
            if(page==0 && ImGui::IsKeyPressed(ImGuiKey_Space))pause(view);
            if(page==0 && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L)){store.clear();detail.reset();}
            if(page==0 && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))exportView(view,false);
            if(page==0 && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
                if(ImGui::GetIO().KeyShift)ImGui::SetClipboardText(viewText(view).c_str());
                else if(auto* e=find(view.selected))ImGui::SetClipboardText(formatLog(*e).c_str());
            }
        }
        topbar();statusbar();
        const auto* viewport=ImGui::GetMainViewport();ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##workspace",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);
        if(ImGui::BeginTable("##workspaceSplit",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Applications",ImGuiTableColumnFlags_WidthFixed,cyui::dp(282));ImGui::TableSetupColumn("Main",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextColumn();ImGui::BeginChild("##sidebar");contextSidebar();ImGui::EndChild();
            ImGui::TableNextColumn();ImGui::BeginChild("##main");if(page==0)console();else if(page==1)applications();else if(page==2)settings();else if(page==3)deviceActions();else if(page==4)terminal();else if(page==5)pluginPage();else if(page==7)filesPage();else if(page==8){metrics->draw();if(metrics->takeConfigureRequest()){for(size_t i=0;i<plugins.size();++i)if(plugins[i].id=="meta.ovr-metrics")selectedPlugin=int(i);page=5;options.pluginTab=0;pluginTabRequest=true;}}else assistantPage();ImGui::EndChild();
            ImGui::EndTable();
        }ImGui::End();if(floatingAssistant){ImGui::SetNextWindowSize(cyui::dp(850,750),ImGuiCond_FirstUseEver);if(ImGui::Begin("Discussion IA",&floatingAssistant))assistantPage();ImGui::End();}modals();cyui::renderNotifications();
    }
};
App::App(Options options) : impl_(std::make_unique<Impl>(std::move(options))) {}
App::~App() = default;
void App::frame(){impl_->frame();}
bool App::readyForUpdate()const{return impl_->options.demo||(impl_->assistantReady&&impl_->updateWorkerReady&&impl_->updateBridge&&impl_->updateBridge->running());}
std::string App::diagnostics() const {
    std::ostringstream out;out<<"demo="<<impl_->options.demo<<"\nonline="<<impl_->online<<"\ncapturing="<<impl_->capturing<<"\npackages="<<impl_->packages.size()<<"\nprocesses="<<impl_->store.processes().size()<<"\nlines="<<impl_->store.entries().size()<<"\nstatus="<<impl_->status<<"\n";
    for(const auto& [package,c]:impl_->counts)if(c.lines)out<<(package.empty()?"[system/unknown]":package)<<" lines="<<c.lines<<" errors="<<c.errors<<"\n";
    size_t selectedLines=0,foreign=0;for(const auto& e:impl_->store.entries()) {if(e.package==impl_->view.package)++selectedLines;}
    for(auto id:impl_->view.ids)if(auto* e=impl_->find(id))if(!impl_->view.package.empty() && impl_->view.package!="@system" && e->package!=impl_->view.package)++foreign;
    out<<"log_raw="<<impl_->view.rawMode<<"\nlog_follow="<<impl_->view.follow<<"\nraw_bytes="<<impl_->view.rawText.size()<<"\n";
    out<<"log_max_lines="<<impl_->store.capacity()<<"\nlog_max_bytes="<<impl_->store.byteLimit()<<"\nfile_workspace="<<impl_->files->describe().dump()<<"\n";
    out<<"selected="<<impl_->view.package<<"\nselected_lines="<<selectedLines<<"\nvisible="<<impl_->view.ids.size()<<"\nforeign_visible="<<foreign<<"\nmodal_open="<<ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)<<"\n";
    return out.str();
}
}
