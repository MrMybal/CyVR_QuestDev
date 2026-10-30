// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/mirroring.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <regex>
#include <thread>

namespace quest {
namespace {
using J=nlohmann::json;
std::wstring quoteArgument(const std::wstring& text){std::wstring result=L"\"";size_t slash=0;for(auto c:text){if(c==L'\\'){++slash;continue;}result.append(slash*(c==L'"'?2:1)+(c==L'"'?1:0),L'\\');slash=0;result+=c;}result.append(slash*2,L'\\');return result+L'"';}
struct Search {DWORD process{};HWND window{};};
HWND videoWindow(DWORD process){Search search{process};EnumWindows([](HWND window,LPARAM parameter)->BOOL{auto& s=*reinterpret_cast<Search*>(parameter);DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid==s.process&&IsWindowVisible(window)&&GetWindow(window,GW_OWNER)==nullptr){s.window=window;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&search));return search.window;}
std::vector<wchar_t> childEnvironment(const std::wstring& adb,const std::filesystem::path& engine){
    std::map<std::wstring,std::wstring> values;auto block=GetEnvironmentStringsW();if(!block)throw std::runtime_error("Environnement Windows indisponible.");
    for(auto* p=block;*p;p+=wcslen(p)+1){std::wstring entry=p;auto at=entry.find(L'=',entry[0]==L'='?1:0);if(at!=std::wstring::npos){auto key=entry.substr(0,at);std::transform(key.begin(),key.end(),key.begin(),::towupper);values[key]=entry.substr(at+1);}}FreeEnvironmentStringsW(block);
    values[L"ADB"]=adb;values[L"SCRCPY_SERVER_PATH"]=(engine.parent_path()/L"scrcpy-server").wstring();
    values[L"SCRCPY_ICON_DIR"]=engine.parent_path().wstring();std::vector<wchar_t> out;
    for(const auto& [key,value]:values){auto item=key+L'='+value;out.insert(out.end(),item.begin(),item.end());out.push_back(0);}out.push_back(0);return out;
}
J description(const MirrorState& s){return {{"active",s.active},{"starting",s.starting},{"stopping",s.stopping},{"serial",s.serial},{"status",s.status},{"error",s.error},{"width",s.width},{"height",s.height},{"fps",s.fps},{"requestedMaxFps",s.options.maxFps},{"requestedMaxSize",s.options.maxSize},{"bitRateMbps",s.options.bitRateMbps},{"audio",s.audioStatus},{"recording",s.recording},{"recordingName",s.recordingFile.filename().u8string()},{"forcedStop",s.forcedStop},{"exitCode",s.exitCode},{"window","dedicated_native_video"}};}
}
std::vector<std::string> mirrorArguments(const std::string& serial,const std::string& title,const MirrorOptions& o){
    if(serial.empty()||serial.size()>256||serial.find_first_of("\r\n\t ")!=std::string::npos||serial.find('\0')!=std::string::npos)throw std::runtime_error("Identifiant de casque invalide.");
    if(o.maxSize<256||o.maxSize>4096||o.maxFps<15||o.maxFps>120||o.bitRateMbps<1||o.bitRateMbps>100||o.audio<0||o.audio>2)throw std::runtime_error("Réglages de mirroring hors limites.");
    std::vector<std::string> args={"--serial="+serial,"--window-title="+title,"--no-control","--no-clipboard-autosync","--no-terminal-title","--video-codec=h264","--hwdec=auto","--max-size="+std::to_string(o.maxSize),"--max-fps="+std::to_string(o.maxFps),"--video-bit-rate="+std::to_string(o.bitRateMbps)+"M","--print-fps"};
    if(o.audio==0)args.push_back("--no-audio");else{args.push_back("--audio-codec=aac");args.push_back(o.audio==1?"--audio-dup":"--audio-source=output");}
    if(o.fullscreen)args.push_back("--fullscreen");if(o.alwaysOnTop)args.push_back("--always-on-top");
    if(o.cropWidth||o.cropHeight||o.cropX||o.cropY){if(o.cropWidth<16||o.cropWidth>8192||o.cropHeight<16||o.cropHeight>8192||o.cropX<0||o.cropY<0||o.cropX>8192||o.cropY>8192)throw std::runtime_error("Recadrage invalide.");args.push_back("--crop="+std::to_string(o.cropWidth)+":"+std::to_string(o.cropHeight)+":"+std::to_string(o.cropX)+":"+std::to_string(o.cropY));}
    if(!o.recording.empty()){if(!o.recording.is_absolute()||lower(o.recording.extension().u8string())!=".mkv")throw std::runtime_error("Un chemin MKV absolu est requis.");if(std::filesystem::exists(o.recording)&&!o.overwriteRecording)throw std::runtime_error("Le fichier d’enregistrement existe déjà.");args.push_back("--record="+o.recording.u8string());args.push_back("--record-format=mkv");}
    return args;
}
struct Mirroring::Impl {
    struct Waiter {uint64_t id{};std::promise<MirrorResult> promise;std::atomic<bool> cancelled{false};};
    mutable std::mutex mutex;MirrorState state;std::thread worker;std::atomic<bool> stopRequested{false};uint64_t nextId{};
    std::map<uint64_t,std::shared_ptr<Waiter>> pending;std::vector<std::shared_ptr<Waiter>> stops;
    ~Impl(){stopRequested=true;if(worker.joinable())worker.join();}
    std::pair<std::shared_ptr<Waiter>,MirrorTicket> waiter(){auto w=std::make_shared<Waiter>();w->id=++nextId;pending[w->id]=w;return {w,{w->id,w->promise.get_future().share()}};}
    void complete(const std::shared_ptr<Waiter>& w,bool ok,const std::string& error={}){J data;{std::lock_guard lock(mutex);if(!pending.erase(w->id))return;data=description(state);}w->promise.set_value({ok&&!w->cancelled,error.empty()&&w->cancelled?"Mirroring annulé.":error,std::move(data)});}
    void line(const std::string& raw){
        auto text=trim(raw);if(text.empty())return;std::lock_guard lock(mutex);state.log+=text+'\n';if(state.log.size()>65536)state.log.erase(0,state.log.size()-65536);
        static const std::regex texture(R"(Texture[^:]*:\s*(\d+)x(\d+))"),fps(R"((\d+) fps)");std::smatch match;
        if(std::regex_search(text,match,texture)){state.width=std::stoi(match[1]);state.height=std::stoi(match[2]);}
        if(std::regex_search(text,match,fps))state.fps=std::stoi(match[1]);
        auto normalized=lower(text);if(normalized.find("audio")!=std::string::npos&&(normalized.find("disabled")!=std::string::npos||normalized.find("failed")!=std::string::npos||normalized.find("not supported")!=std::string::npos))state.audioStatus="unavailable";
    }
    void run(std::wstring engine,std::wstring adb,std::vector<std::string> args,std::shared_ptr<Waiter> start){
        HANDLE read=nullptr,write=nullptr,input=INVALID_HANDLE_VALUE,job=nullptr;PROCESS_INFORMATION process{};bool started=false,ready=false,closing=false,forced=false;std::string failure,buffer;DWORD code=1;
        auto finish=[&]{
            if(write)CloseHandle(write);if(read)CloseHandle(read);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
            if(process.hThread)CloseHandle(process.hThread);if(process.hProcess)CloseHandle(process.hProcess);if(job)CloseHandle(job);
            std::vector<std::shared_ptr<Waiter>> done;{std::lock_guard lock(mutex);state.active=state.starting=state.stopping=state.recording=false;state.processId=0;state.exitCode=int(code);state.forcedStop=forced;state.error=failure;state.status=failure.empty()?"Mirroring arrêté.":"Mirroring interrompu.";done.swap(stops);}
            if(!ready)complete(start,false,failure.empty()?"Démarrage du mirroring annulé.":failure);
            for(const auto& w:done)complete(w,failure.empty()&&!forced,failure);
        };
        try{
            SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};if(!CreatePipe(&read,&write,&security,0))throw std::runtime_error("Canal vidéo indisponible.");SetHandleInformation(read,HANDLE_FLAG_INHERIT,0);
            input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
            STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdInput=input;startup.hStdOutput=startup.hStdError=write;
            auto environment=childEnvironment(adb,engine);std::wstring command=quoteArgument(engine);for(const auto& arg:args)command+=L' '+quoteArgument(widen(arg));
            job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))throw std::runtime_error("Isolation du processus vidéo impossible.");
            if(!CreateProcessW(engine.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,environment.data(),std::filesystem::path(engine).parent_path().c_str(),&startup,&process))throw std::runtime_error("Le moteur vidéo ne démarre pas (Windows "+std::to_string(GetLastError())+").");
            started=true;if(!AssignProcessToJobObject(job,process.hProcess)){TerminateProcess(process.hProcess,1);throw std::runtime_error("Isolation du moteur vidéo refusée.");}
            {std::lock_guard lock(mutex);state.processId=process.dwProcessId;}ResumeThread(process.hThread);CloseHandle(process.hThread);process.hThread=nullptr;CloseHandle(write);write=nullptr;CloseHandle(input);input=INVALID_HANDLE_VALUE;
            auto beginning=std::chrono::steady_clock::now(),closeTime=beginning;
            for(;;){
                DWORD available=0;if(PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr)&&available){char bytes[8192];DWORD n=0;if(ReadFile(read,bytes,std::min<DWORD>(available,sizeof(bytes)),&n,nullptr)&&n){buffer.append(bytes,n);size_t pos;while((pos=buffer.find_first_of("\r\n"))!=std::string::npos){line(buffer.substr(0,pos));buffer.erase(0,pos+1);}if(buffer.size()>16384){line(buffer.substr(0,16384));buffer.clear();}}}
                auto now=std::chrono::steady_clock::now();auto window=videoWindow(process.dwProcessId);
                if(!ready&&!closing&&!start->cancelled&&!stopRequested){bool texture=false;{std::lock_guard lock(mutex);texture=state.width>0&&state.height>0;}if(texture&&window){ready=true;{std::lock_guard lock(mutex);state.starting=false;state.status="Diffusion vidéo en cours.";}complete(start,true);}}
                if((stopRequested||(!ready&&start->cancelled))&&!closing){closing=true;closeTime=now;{std::lock_guard lock(mutex);state.stopping=true;state.status="Arrêt et finalisation du flux…";}if(window)PostMessageW(window,WM_CLOSE,0,0);}
                if(!ready&&!closing&&now-beginning>std::chrono::seconds(20)){failure="Aucune image vidéo reçue. Consultez le journal du mirroring.";closing=true;closeTime=now;if(window)PostMessageW(window,WM_CLOSE,0,0);}
                if(WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0)break;
                if(closing&&now-closeTime>std::chrono::seconds(5)){forced=true;TerminateJobObject(job,1);WaitForSingleObject(process.hProcess,2000);break;}
                Sleep(10);
            }
            if(!buffer.empty())line(buffer);GetExitCodeProcess(process.hProcess,&code);
            if(!ready&&failure.empty()&&!stopRequested&&!start->cancelled)failure="Le moteur vidéo a quitté avant la première image.";
            if(ready&&code!=0&&failure.empty())failure="La diffusion a été interrompue (code "+std::to_string(code)+").";
            if(forced)failure="Arrêt forcé du moteur vidéo. L’enregistrement peut être incomplet.";
        }catch(const std::exception& e){failure=e.what();if(started&&process.hProcess){TerminateJobObject(job,1);WaitForSingleObject(process.hProcess,2000);}}
        finish();
    }
};
Mirroring::Mirroring():impl_(std::make_unique<Impl>()){}
Mirroring::~Mirroring()=default;
MirrorTicket Mirroring::start(const std::wstring& engine,const std::wstring& adb,const std::string& serial,const MirrorOptions& options){
    auto args=mirrorArguments(serial,"CyVR QuestDev - "+serial,options);
    if(!std::filesystem::is_regular_file(engine)||!std::filesystem::is_regular_file(adb))throw std::runtime_error("Moteur scrcpy ou ADB absent. Vérifiez les réglages et le paquet installé.");
    {std::lock_guard lock(impl_->mutex);if(impl_->state.active)throw std::runtime_error("Un mirroring est déjà actif.");}
    if(impl_->worker.joinable())impl_->worker.join();std::shared_ptr<Impl::Waiter> start;MirrorTicket ticket;
    {std::lock_guard lock(impl_->mutex);auto pair=impl_->waiter();start=pair.first;ticket=pair.second;impl_->stopRequested=false;impl_->state=MirrorState{};auto& s=impl_->state;s.active=s.starting=true;s.options=options;s.serial=serial;s.title="CyVR QuestDev - "+serial;s.status="Connexion du flux vidéo…";s.recording=!options.recording.empty();s.recordingFile=options.recording;s.audioStatus=options.audio==0?"off":options.audio==1?"requested_duplicate":"requested_pc_output";}
    impl_->worker=std::thread([this,engine,adb,args=std::move(args),start]{impl_->run(engine,adb,args,start);});return ticket;
}
MirrorTicket Mirroring::stop(){std::shared_ptr<Impl::Waiter> waiter;MirrorTicket ticket;bool idle;{std::lock_guard lock(impl_->mutex);auto pair=impl_->waiter();waiter=pair.first;ticket=pair.second;idle=!impl_->state.active;if(!idle){impl_->stops.push_back(waiter);impl_->stopRequested=true;}}if(idle)impl_->complete(waiter,true);return ticket;}
void Mirroring::cancel(uint64_t id){std::lock_guard lock(impl_->mutex);auto at=impl_->pending.find(id);if(at!=impl_->pending.end())at->second->cancelled=true;}
void Mirroring::focus(){auto s=state();if(auto window=videoWindow(s.processId)){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);}}
MirrorState Mirroring::state()const{std::lock_guard lock(impl_->mutex);return impl_->state;}
nlohmann::json Mirroring::describe()const{return description(state());}
}
