// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/backend.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace quest {
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0); std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n); return out;
}
std::string narrow(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr); std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n, nullptr, nullptr); return out;
}
static std::wstring env(const wchar_t* key) { wchar_t b[32768]{}; auto n = GetEnvironmentVariableW(key,b,32768); return n && n < 32768 ? b : L""; }
std::wstring findAdb() {
    wchar_t exe[32768]{}; GetModuleFileNameW(nullptr, exe, 32768);
    std::vector<std::filesystem::path> roots{std::filesystem::path(exe).parent_path(), std::filesystem::path(exe).parent_path()/L"platform-tools"};
    for (const auto* key : {L"ANDROID_HOME", L"ANDROID_SDK_ROOT"}) { auto s = env(key); if (!s.empty()) roots.emplace_back(std::filesystem::path(s)/L"platform-tools"); }
    auto local = env(L"LOCALAPPDATA"); if (!local.empty()) roots.emplace_back(std::filesystem::path(local)/L"Android/Sdk/platform-tools");
    for (const auto& root : roots) { std::error_code ec; auto p = root / L"adb.exe"; if (std::filesystem::is_regular_file(p,ec)) return p.wstring(); }
    wchar_t path[32768]{}; if (SearchPathW(nullptr,L"adb.exe",nullptr,32768,path,nullptr)) return path; return L"";
}
// Windows CRT argument quoting, including trailing backslashes and quotes.
static std::wstring quote(const std::wstring& arg) {
    std::wstring out = L"\""; size_t slashes=0;
    for (wchar_t c : arg) { if (c == L'\\') { ++slashes; continue; } if (c == L'"') { out.append(slashes*2+1,L'\\'); out += c; } else { out.append(slashes,L'\\'); out+=c; } slashes=0; }
    out.append(slashes*2,L'\\'); out+=L'"'; return out;
}
CommandResult runProcess(const std::wstring& exe, const std::vector<std::string>& args, int timeoutMs,
    const std::atomic<bool>* cancel, const std::function<void(const std::string&)>& chunk, const std::wstring& outputFile) {
    CommandResult out;
    if (exe.empty()) { out.output = "ADB introuvable. Choisissez adb.exe dans les réglages."; return out; }
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE}; HANDLE read{}, write{};
    if (!CreatePipe(&read,&write,&security,0)) { out.output = "Impossible de créer le canal ADB."; return out; }
    SetHandleInformation(read,HANDLE_FLAG_INHERIT,0);
    HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    HANDLE binary=INVALID_HANDLE_VALUE;
    if(!outputFile.empty())binary=CreateFileW(outputFile.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(!outputFile.empty() && binary==INVALID_HANDLE_VALUE){CloseHandle(read);CloseHandle(write);if(nullInput!=INVALID_HANDLE_VALUE)CloseHandle(nullInput);out.output="Impossible d’ouvrir le fichier de sortie.";return out;}
    STARTUPINFOW si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=binary!=INVALID_HANDLE_VALUE?binary:write; si.hStdError=write; si.hStdInput=nullInput;
    PROCESS_INFORMATION pi{}; std::wstring command=quote(exe); for (const auto& a:args) command+=L" "+quote(widen(a));
    BOOL started = CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);
    DWORD err=GetLastError(); CloseHandle(write);if(binary!=INVALID_HANDLE_VALUE)CloseHandle(binary);if (nullInput != INVALID_HANDLE_VALUE) CloseHandle(nullInput);
    if (!started) { CloseHandle(read); out.output="Impossible de lancer ADB (Windows "+std::to_string(err)+")."; return out; }
    CloseHandle(pi.hThread); auto begin=std::chrono::steady_clock::now(); bool terminated=false;
    for (;;) {
        DWORD available{};
        if (PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr) && available) {
            char buffer[16384]; DWORD n{};
            if (ReadFile(read,buffer,std::min(available,DWORD(sizeof(buffer))),&n,nullptr) && n) {
                std::string data(buffer,n); if (chunk) chunk(data); else if (out.output.size()<8*1024*1024) out.output+=data;
            }
            // Cancellation is checked even under continuous high-volume output.
        } else if (WaitForSingleObject(pi.hProcess,0)==WAIT_OBJECT_0) break;
        else Sleep(8);
        bool timeout = timeoutMs > 0 && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count()>timeoutMs;
        if (!terminated && (timeout || (cancel && *cancel))) { out.timedOut=timeout; TerminateProcess(pi.hProcess,1); terminated=true; }
        if (terminated && WaitForSingleObject(pi.hProcess,100)==WAIT_OBJECT_0) break;
    }
    DWORD code{}; GetExitCodeProcess(pi.hProcess,&code); out.code=int(code); CloseHandle(pi.hProcess); CloseHandle(read);
    if (out.timedOut) out.output += "\nDélai ADB dépassé."; return out;
}
Backend::Backend(std::wstring adb) : adb_(std::move(adb)), worker_([this]{loop();}) {}
Backend::~Backend() {
    quit_=true;stopStream_=true;wake_.notify_all();
    {std::lock_guard<std::mutex> lock(jobsMutex_);for(auto& job:jobs_)job->cancel=true;}
    if(worker_.joinable())worker_.join();
    for(auto& job:jobs_)if(job->thread.joinable())job->thread.join();
}
uint64_t Backend::select(const std::string& serial) {
    std::lock_guard<std::mutex> lock(mutex_); desired_=serial; auto id=++session_; refresh_=true; stopStream_=true; requests_.clear();
    {std::lock_guard<std::mutex> jobsLock(jobsMutex_);for(auto& job:jobs_)job->cancel=true;}wake_.notify_all(); return id;
}
void Backend::refresh() { std::lock_guard<std::mutex> lock(mutex_); refresh_=true; wake_.notify_all(); }
void Backend::setAdb(std::wstring path) { std::lock_guard<std::mutex> lock(mutex_); adb_=std::move(path); ++session_; desired_.clear(); requests_.clear(); refresh_=true; stopStream_=true;{std::lock_guard<std::mutex> jobsLock(jobsMutex_);for(auto& job:jobs_)job->cancel=true;}wake_.notify_all(); }
void Backend::action(std::string action, std::string value) {
    std::lock_guard<std::mutex> lock(mutex_); if(requests_.size()<16) requests_.push_back({std::move(action),std::move(value),session_.load()}); wake_.notify_all();
}
std::string shellQuote(const std::string& text) {
    std::string out="'";for(char c:text)out+=c=='\''?"'\\''":std::string(1,c);return out+"'";
}
std::vector<std::string> parseCommand(const std::string& text, std::string& error) {
    error.clear();std::vector<std::string> out;std::string word;char quoteChar=0;bool started=false;
    for(size_t i=0;i<text.size();++i){char c=text[i];if(c=='\0'||c=='\r'||c=='\n'){error="Une seule commande est attendue.";return {};}
        if(quoteChar){if(c==quoteChar)quoteChar=0;else if(c=='\\' && i+1<text.size() && (text[i+1]==quoteChar || text[i+1]=='\\'))word+=text[++i];else word+=c;}
        else if(c=='\''||c=='"'){quoteChar=c;started=true;}else if(std::isspace(static_cast<unsigned char>(c))){if(started){out.push_back(word);word.clear();started=false;}}
        else {word+=c;started=true;}
    }
    if(quoteChar){error="Guillemet non fermé.";return {};}if(started)out.push_back(word);return out;
}
uint64_t Backend::submit(JobRequest request) {
    std::wstring adb;std::string serial;uint64_t sid;
    {std::lock_guard<std::mutex> lock(mutex_);adb=adb_;serial=desired_;sid=session_;}
    std::lock_guard<std::mutex> lock(jobsMutex_);
    for(auto it=jobs_.begin();it!=jobs_.end();)if((*it)->done){if((*it)->thread.joinable())(*it)->thread.join();it=jobs_.erase(it);}else ++it;
    const auto id=nextJob_++;
    if(jobs_.size()>=8 || request.steps.empty() || (request.needsDevice&&serial.empty())) {
        BackendEvent e;e.kind=BackendEvent::JobDone;e.job=id;e.session=sid;e.label=request.label;e.code=-1;e.text=serial.empty()?"Sélectionnez un casque.":"Trop de tâches simultanées, ou commande vide.";push(std::move(e));return id;
    }
    auto job=std::make_shared<Job>();job->id=id;job->session=sid;jobs_.push_back(job);
    job->thread=std::thread([this,job,adb,serial,request=std::move(request)] {
        auto event=[&](BackendEvent::Kind kind,std::string text,int code=0){BackendEvent e;e.kind=kind;e.job=job->id;e.session=job->session;e.label=request.label;e.text=std::move(text);e.code=code;e.ok=code==0;e.cancelled=job->cancel;push(std::move(e));};
        event(BackendEvent::JobStarted,request.label);int code=0;std::string result;
        for(const auto& step:request.steps) {
            if(job->cancel || job->session!=session_){job->cancel=true;code=1;break;}
            auto args=step.args;if(request.needsDevice)args.insert(args.begin(),{"-s",serial});
            std::string cmd="adb";for(const auto& a:args)cmd+=" "+a;event(BackendEvent::JobOutput,"$ "+cmd+"\n");
            size_t bytes=0;auto chunk=[&](const std::string& s){if(bytes<1024*1024){event(BackendEvent::JobOutput,s.substr(0,1024*1024-bytes));bytes+=s.size();}};
            auto r=runProcess(adb,args,request.timeoutMs,&job->cancel,chunk,step.outputFile);code=r.code;
            if(r.timedOut)event(BackendEvent::JobOutput,"\nDélai dépassé.\n");if(code!=0){result=r.output;break;}
        }
        if(job->session!=session_)job->cancel=true;
        if(code!=0||job->cancel)for(const auto& step:request.cleanupSteps){
            auto args=step.args;if(request.needsDevice)args.insert(args.begin(),{"-s",serial});
            // Cleanup remains scoped to the original device and is acknowledged
            // before JobDone, even when the original cancellation flag is set.
            auto cleanup=runProcess(adb,args,5000,nullptr,[&](const std::string& text){event(BackendEvent::JobOutput,text);});
            if(cleanup.code!=0)event(BackendEvent::JobOutput,"\nNettoyage distant incomplet : "+cleanup.output+"\n");
        }
        event(BackendEvent::JobDone,job->cancel?"Tâche annulée.":code==0?"Terminé.":"Échec de la commande. "+result,job->cancel?1:code);job->done=true;
    });return id;
}
void Backend::cancel(uint64_t id) {std::lock_guard<std::mutex> lock(jobsMutex_);for(auto& job:jobs_)if(job->id==id)job->cancel=true;}
void Backend::push(BackendEvent e) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    if(e.kind==BackendEvent::JobOutput){if(queuedBytes_+e.text.size()>16*1024*1024)return;queuedBytes_+=e.text.size();}
    if (e.kind==BackendEvent::Lines) {
        size_t n=0; for(const auto& s:e.lines)n+=s.size();
        if(queuedBytes_+n>16*1024*1024) { dropped_+=e.lines.size(); return; } queuedBytes_+=n;
    }
    events_.push_back(std::move(e));
}
std::vector<BackendEvent> Backend::poll() {
    std::lock_guard<std::mutex> lock(eventMutex_); std::vector<BackendEvent> out;
    while(!events_.empty()) { out.push_back(std::move(events_.front())); events_.pop_front(); } queuedBytes_=0; return out;
}
void Backend::stopCapture() { stopStream_=true; if(stream_.joinable())stream_.join();streamEnded_=false; }
static double epochFrom(const std::string& text) { try { auto t=std::stod(trim(text)); return std::isfinite(t) && t>1e9 ? t : 0; } catch(...) {return 0;} }
void Backend::loop() {
    using Clock=std::chrono::steady_clock;
    std::string active; uint64_t activeSession=0; auto nextScan=Clock::now(), nextProcesses=Clock::now(), nextPackages=Clock::now(),retryAfter=Clock::now();
    while(!quit_) {
        std::wstring adb; std::string desired; bool refresh=false; uint64_t sid; std::deque<Request> requests;
        {
            std::unique_lock<std::mutex> lock(mutex_); wake_.wait_for(lock,std::chrono::milliseconds(100),[&]{return quit_ || refresh_ || !requests_.empty();});
            adb=adb_; desired=desired_; sid=session_; refresh=refresh_; refresh_=false; requests.swap(requests_);
        }
        if(quit_)break;
        if(sid!=activeSession) { stopCapture(); active.clear(); activeSession=sid;retryAfter=Clock::now(); }
        if(streamEnded_.exchange(false)){stopCapture();retryAfter=Clock::now()+std::chrono::seconds(3);}
        auto run=[&](std::vector<std::string> args,int timeout=10000) { return runProcess(adb,args,timeout,&quit_); };
        auto shell=[&](std::vector<std::string> args,int timeout=10000) { args.insert(args.begin(),{"-s",desired,"shell"}); return run(args,timeout); };
        if(refresh || Clock::now()>=nextScan) {
            auto result=run({"devices","-l"}); BackendEvent e; e.kind=BackendEvent::Devices; e.devices=parseDevices(result.output); e.ok=result.code==0; e.text=e.ok?"":result.output; push(std::move(e));
            nextScan=Clock::now()+std::chrono::seconds(3);
            bool online=false; for(const auto& d:parseDevices(result.output))if(d.serial==desired && d.state=="device")online=true;
            if(!online && !active.empty()) { stopCapture(); active.clear(); BackendEvent status;status.session=sid;status.text="Casque déconnecté. Reconnexion automatique en attente.";push(std::move(status)); }
            if(online && active.empty() && sid==session_) {
                active=desired; nextPackages=nextProcesses=Clock::now();
            }
        }
        for(auto it=requests.begin();it!=requests.end();) {
            if(it->action!="connect"){++it;continue;}
            const auto value=it->value;CommandResult result;
            if(!value.empty() && std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='.'||c==':'||c=='-'||c=='['||c==']';})) result=run({"connect",value});
            else { result.code=1;result.output="Adresse ADB invalide."; }
            BackendEvent done;done.kind=BackendEvent::Operation;done.session=sid;done.ok=result.code==0 && result.output.find("cannot")==std::string::npos && result.output.find("failed")==std::string::npos;done.text=result.output;push(std::move(done));nextScan=Clock::now();it=requests.erase(it);
        }
        if(active.empty() || sid!=session_) {
            for(const auto& request:requests) {BackendEvent done;done.kind=BackendEvent::Operation;done.session=request.session;done.text="Casque indisponible. Reconnectez-le puis réessayez.";push(std::move(done));}
            continue;
        }
        if(refresh){retryAfter=Clock::now();if(stream_.joinable())stopCapture();}
        if(refresh || Clock::now()>=nextPackages) {
            auto all=shell({"pm","list","packages","-U"}); auto system=shell({"pm","list","packages","-s"});
            BackendEvent e; e.kind=BackendEvent::Packages;e.session=sid;e.ok=all.code==0;e.packages=parsePackages(all.output);
            std::set<std::string> systemNames; for(const auto& p:parsePackages(system.output,true))systemNames.insert(p.name);
            for(auto& p:e.packages)p.system=systemNames.count(p.name)>0;
            if(!e.ok)e.text=all.output; push(std::move(e)); nextPackages=Clock::now()+std::chrono::seconds(20);
        }
        if(refresh || Clock::now()>=nextProcesses || (!stream_.joinable() && Clock::now()>=retryAfter)) {
            auto time=shell({"date","+%s.%N"}); double epoch=epochFrom(time.output);
            auto ps=shell({"ps","-A","-o","PID,UID,NAME"}); auto processes=parseProcesses(ps.output);
            if(epoch==0 || ps.code!=0 || processes.empty()) {
                BackendEvent e;e.session=sid;e.text="Lecture des processus impossible : "+ps.output+"\n"+time.output;push(std::move(e)); nextProcesses=Clock::now()+std::chrono::seconds(3);continue;
            }
            BackendEvent e;e.kind=BackendEvent::Processes;e.session=sid;e.epoch=epoch;e.processes=std::move(processes);push(std::move(e));
            nextProcesses=Clock::now()+std::chrono::seconds(2);
            if(!stream_.joinable() && Clock::now()>=retryAfter) {
                stopStream_=false;
                std::ostringstream since;since<<std::fixed<<std::setprecision(6)<<epoch;
                auto serial=active;
                stream_=std::thread([this,adb,serial,sid,since=since.str()] {
                    BackendEvent state;state.session=sid;state.ok=true;state.text="Capture en direct";push(std::move(state));
                    std::string pending, diagnostic;
                    auto consume=[&](const std::string& chunk) {
                        pending+=chunk;BackendEvent lines;lines.kind=BackendEvent::Lines;lines.session=sid;
                        size_t p=0,start=0;
                        while((p=pending.find('\n',start))!=std::string::npos) { auto line=pending.substr(start,p-start);if(line.size()<256*1024) { if(!parseLog(line) && line.rfind("---------",0)!=0)diagnostic=trim(line);lines.lines.push_back(std::move(line)); } else ++dropped_;start=p+1; }
                        pending.erase(0,start);if(pending.size()>256*1024){pending.clear();++dropped_;}
                        if(!lines.lines.empty())push(std::move(lines));
                    };
                    auto result=runProcess(adb,{"-s",serial,"logcat","-b","main","-b","system","-b","crash","-b","events","-v","threadtime,epoch,usec","-T",since,"*:V"},0,&stopStream_,consume);
                    if(!pending.empty())consume("\n");
                    if(!stopStream_) { BackendEvent e;e.session=sid;e.text="Capture interrompue (ADB "+std::to_string(result.code)+"). "+diagnostic+" Nouvelle tentative dans 3 s.";push(std::move(e));streamEnded_=true; }
                });
            }
        }
        for(const auto& request:requests) {
            if(request.session!=session_ || desired!=active)continue;
            CommandResult r; const auto& value=request.value;
            if(request.action=="launch" && validPackage(value)) {
                auto resolve=shell({"cmd","package","resolve-activity","--brief","-a","android.intent.action.MAIN","-c","android.intent.category.LAUNCHER",value});
                std::istringstream s(resolve.output);std::string line,component;
                while(std::getline(s,line)) { line=trim(line); if(line.find('/')!=std::string::npos && std::all_of(line.begin(),line.end(),[](unsigned char c){return std::isalnum(c)||c=='.'||c=='_'||c=='/'||c=='$';}))component=line; }
                if(component.empty()){r.code=-1;r.output="Aucune activité de lancement pour "+value+".\n"+resolve.output;}
                else r=shell({"am","start","-n",component});
                if(r.output.find("Error")!=std::string::npos)r.code=1;
            } else if(request.action=="stop" && validPackage(value))r=shell({"am","force-stop",value});
            else if(request.action=="install")r=run({"-s",active,"install","-r",value},180000);
            else if(request.action=="connect" && !value.empty() && std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='.'||c==':'||c=='-'||c=='['||c==']';}))r=run({"connect",value});
            else { r.code=-1;r.output="Commande ou identifiant invalide."; }
            BackendEvent done;done.kind=BackendEvent::Operation;done.session=sid;done.ok=r.code==0;done.text=r.output.empty()?(done.ok?"Opération terminée.":"Échec de l’opération."):r.output;push(std::move(done));nextPackages=nextProcesses=Clock::now();
        }
    }
    stopCapture();
}
}
