// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/metrics_capture.hpp"
#include "quest/metrics_plugins.hpp"
#include "quest/files.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <cmath>

namespace quest {
using J=nlohmann::json;
namespace {
const std::vector<std::string> directories={"/sdcard/Android/data/com.oculus.ovrmonitormetricsservice/files/CapturedMetrics","/sdcard/OVRMonitorMetricsService/CapturedMetrics"};
struct Endpoint {std::wstring adb;std::string serial;uint64_t session{};bool online{};};
bool same(const Endpoint& a,const Endpoint& b){return a.adb==b.adb&&a.serial==b.serial&&a.session==b.session&&a.online==b.online;}
std::string bytes(const std::filesystem::path& path,size_t maximum){std::error_code ec;auto size=std::filesystem::file_size(path,ec);if(ec||size>maximum)throw std::runtime_error("Fichier absent ou trop volumineux.");std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Lecture du fichier impossible.");return std::string((std::istreambuf_iterator<char>(input)),{});}
void atomicWrite(const std::filesystem::path& path,const std::string& value){auto temporary=path;temporary+=L".part";std::ofstream output(temporary,std::ios::binary);output.write(value.data(),std::streamsize(value.size()));output.close();if(!output||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Sauvegarde du fichier impossible.");}
Plugin ovr(){auto p=parsePlugin("[plugin]\napi=1\nid=meta.ovr-metrics\nname=OVR Metrics\npackage=com.oculus.ovrmonitormetricsservice\ncontroller=ovr_metrics");return p;}
std::string reportPath(const std::string& path){auto normalized=remotePath(path);bool scoped=false;for(const auto& root:directories)if(normalized.rfind(root+"/",0)==0)scoped=true;if(!scoped||normalized.size()<4||lower(normalized).substr(normalized.size()-4)!=".csv")throw std::runtime_error("Un rapport CSV du dossier OVR Metrics est requis.");return normalized;}
std::string safeName(const std::string& name){std::string value;for(unsigned char c:name){if(c<32||c==127||std::string("<>:\"/\\|?*").find(char(c))!=std::string::npos)value+='_';else value+=char(c);}if(value.size()>120)value.resize(120);while(!value.empty()&&(value.back()=='.'||value.back()==' '))value.pop_back();return value.empty()?"report.csv":value;}
J summary(const MetricsState& s){J columns=J::array(),reports=J::array();if(s.data)for(const auto& column:s.data->columns)columns.push_back({{"id",column},{"unit",metricsUnit(column)}});for(size_t i=0;i<std::min<size_t>(100,s.reports.size());++i){const auto& report=s.reports[i];reports.push_back({{"path",report.path},{"name",report.name},{"bytes",report.size},{"modified",report.modified}});}return {{"source",s.data?s.data->name:""},{"sourceDevice",s.sourceDevice},{"rows",s.data?s.data->times.size():0},{"duration",s.data&&!s.data->times.empty()?s.data->times.back():0},{"columns",columns},{"reports",reports},{"reportsCount",s.reports.size()},{"reportsTruncated",s.reports.size()>100},{"remote",s.remote},{"recording",s.recording},{"recordingId",s.recordingId},{"savedFiles",s.savedFiles},{"follow",s.follow},{"busy",s.busy},{"online",s.online},{"status",s.status},{"error",s.error},{"diagnostic",s.diagnostic},{"reportDirectories",s.reportDirectories},{"timeBasis","seconds_from_first_csv_timestamp"}};}
}
std::string metricsReportScript(){
    // One shell invocation covers both OVR generations and session subfolders.
    // find does not follow symbolic links. NUL framing preserves arbitrary names.
    std::string script="printf 'CYVROVR2\\000'; for d in";for(const auto& root:directories)script+=" "+shellQuote(root);
    script+="; do if test ! -d \"$d\"; then continue; fi; if test ! -r \"$d\" || test ! -x \"$d\"; then printf 'DENIED\\000%s\\000' \"$d\"; continue; fi; printf 'ROOT\\000%s\\000' \"$d\"; "
        "find \"$d\" -maxdepth 4 -type f -iname '*.csv' -exec sh -c 'for p do v=$(stat -c \"%s %Y\" \"$p\") || exit 1; printf \"FILE\\000%s\\000%s\\000%s\\000\" \"${v%% *}\" \"${v#* }\" \"$p\"; done' sh {} + || exit 1; done; printf 'END\\000'";
    return script;
}
MetricsReportListing parseMetricsReports(const std::string& text){
    if(text.size()>4*1024*1024)throw std::runtime_error("Liste OVR trop volumineuse.");size_t at=0;auto field=[&]{auto end=text.find('\0',at);if(end==std::string::npos)throw std::runtime_error("Liste OVR incomplète.");auto value=text.substr(at,end-at);at=end+1;return value;};
    auto number=[&]{auto value=field();if(value.empty()||value.size()>19||!std::all_of(value.begin(),value.end(),[](char c){return c>='0'&&c<='9';}))throw std::runtime_error("Métadonnées OVR invalides.");return std::stoull(value);};
    if(field()!="CYVROVR2")throw std::runtime_error("Liste OVR non reconnue.");MetricsReportListing result;std::set<std::string> paths;
    for(;;){auto type=field();if(type=="END"){if(at!=text.size())throw std::runtime_error("Données après la liste OVR.");break;}
        if(type=="ROOT"||type=="DENIED"){auto root=field();if(std::find(directories.begin(),directories.end(),root)==directories.end())throw std::runtime_error("Dossier OVR inattendu.");(type=="ROOT"?result.directories:result.denied).push_back(root);continue;}
        if(type!="FILE"||result.reports.size()>=10000)throw std::runtime_error("Entrée OVR invalide ou limite de 10 000 rapports atteinte.");
        auto size=number(),modified=number();auto path=reportPath(field());if(modified>INT64_MAX||!paths.insert(path).second)throw std::runtime_error("Métadonnées OVR incohérentes.");auto name=path.substr(path.rfind('/')+1);result.reports.push_back({path,name,size,int64_t(modified)});
    }
    std::sort(result.reports.begin(),result.reports.end(),[](const auto& a,const auto& b){return a.modified!=b.modified?a.modified>b.modified:a.path<b.path;});return result;
}
struct MetricsCapture::Impl {
    struct Command {uint64_t id{};std::string action,remote;std::filesystem::path path;int seconds{};std::promise<MetricsResult> promise;std::shared_ptr<std::atomic<bool>> cancel=std::make_shared<std::atomic<bool>>(false);};
    std::filesystem::path cache;bool demo{};
    mutable std::mutex mutex;std::condition_variable wake;MetricsState state;Endpoint desired,observed,recordDevice;
    std::deque<std::shared_ptr<Command>> queue;std::map<uint64_t,std::weak_ptr<Command>> pending;
    std::shared_ptr<std::atomic<bool>> interrupt;std::thread worker;bool quit{};uint64_t sequence{},fileSequence{};
    bool recording{},ownedCsv{},following{},autoSelectRecording{};std::string selectedRemote,selectedSignature;std::filesystem::path recordFolder;
    std::map<std::string,std::string> baseline,savedSignatures;std::map<std::string,std::filesystem::path> savedPaths;
    std::map<std::string,bool> savedSamples;
    std::shared_ptr<Command> timed;std::chrono::steady_clock::time_point endTime{},nextPoll{},nextDiagnostic{},recordStarted{};
    explicit Impl(std::filesystem::path location,bool simulation):cache(std::move(location)),demo(simulation){
        if(demo){auto data=std::make_shared<MetricsCsv>(parseMetricsCsv(demoMetricsCsv()));data->name="Démonstration OVR · données simulées.csv";state.data=data;state.dataRevision=1;state.status="Démonstration · données simulées";}
        worker=std::thread([this]{loop();});
    }
    ~Impl(){{std::lock_guard lock(mutex);quit=true;if(interrupt)*interrupt=true;if(timed)*timed->cancel=true;for(auto& [id,weak]:pending)if(auto command=weak.lock())*command->cancel=true;}wake.notify_all();if(worker.joinable())worker.join();}
    void update(const std::function<void(MetricsState&)>& change){std::lock_guard lock(mutex);change(state);}
    void complete(const std::shared_ptr<Command>& command,MetricsResult result){if(!command)return;{std::lock_guard lock(mutex);pending.erase(command->id);}command->promise.set_value(std::move(result));}
    void execute(const Endpoint& endpoint,const JobRequest& job,const std::atomic<bool>* cancellation){
        if(endpoint.serial.empty()||endpoint.adb.empty())throw std::runtime_error("Aucun casque sélectionné.");
        for(const auto& step:job.steps){if(cancellation&&*cancellation)throw std::runtime_error("Opération annulée.");auto args=step.args;if(job.needsDevice)args.insert(args.begin(),{"-s",endpoint.serial});auto result=runProcess(endpoint.adb,args,job.timeoutMs,cancellation,{},step.outputFile);if(result.code||result.timedOut)throw std::runtime_error(result.output.empty()?"La commande ADB a échoué.":result.output.substr(0,2000));}
    }
    std::string output(const Endpoint& endpoint,const JobRequest& job,const std::atomic<bool>* cancellation){
        std::string text;for(const auto& step:job.steps){auto args=step.args;args.insert(args.begin(),{"-s",endpoint.serial});auto result=runProcess(endpoint.adb,args,std::min(job.timeoutMs,10000),cancellation);if(result.code||result.timedOut)throw std::runtime_error(result.output.empty()?"OVR Metrics ne répond pas. Vérifiez que le service est installé et que le Quest est autorisé via ADB.":result.output.substr(0,2000));text+=result.output;}if(cancellation&&*cancellation)throw std::runtime_error("Opération annulée.");return text;
    }
    J csv(const Endpoint& endpoint,const std::string& action,const std::atomic<bool>* cancellation){auto p=ovr();auto job=metricsPluginJob(p,action);job.timeoutMs=10000;return metricsPluginResult(p,action,output(endpoint,job,cancellation));}
    std::string signature(const MetricsReport& report){return std::to_string(report.size)+":"+std::to_string(report.modified);}
    std::vector<MetricsReport> scan(const Endpoint& endpoint,const std::atomic<bool>* cancellation){
        std::filesystem::create_directories(cache);auto destination=cache/L"reports.bin";JobRequest job{"Chercher les rapports OVR",{{{"exec-out","sh","-c",metricsReportScript()},destination.wstring()}},10000,true};
        std::string data;try{execute(endpoint,job,cancellation);data=bytes(destination,4*1024*1024);}catch(...){std::error_code ec;std::filesystem::remove(destination,ec);throw;}std::error_code ec;std::filesystem::remove(destination,ec);
        auto listing=parseMetricsReports(data);update([&](auto& s){s.reportDirectories=listing.directories;});
        if(!listing.denied.empty())throw std::runtime_error("Accès refusé au dossier OVR : "+listing.denied.front());
        return listing.reports;
    }
    void diagnose(const Endpoint& endpoint,const std::atomic<bool>* cancellation){
        auto config=csv(endpoint,"read_state",cancellation).at("configuration");std::string message;
        if(!config.contains("csv"))message="OVR ne fournit pas son état CSV. Vérifiez la version du service.";
        else if(!config["csv"].get<bool>())message="Le mode CSV est désactivé sur le Quest. Démarrez un enregistrement ou activez-le dans le plugin OVR.";
        else if(config.value("stats",std::vector<std::string>{}).empty())message="Aucune statistique sélectionnée dans OVR. Configurez les métriques dans le plugin OVR (onglet Stats), puis ouvrez une session VR.";
        else message="CSV confirmé actif. OVR publie ses rapports selon la session VR ; certains rapports n’apparaissent qu’à sa fin. Vérifiez que le casque est éveillé et que votre application VR est ouverte.";
        update([&](auto& s){s.diagnostic=(s.reportDirectories.empty()?"Aucun dossier de rapports OVR détecté dans les deux emplacements pris en charge. ":std::string())+message;});nextDiagnostic=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    }
    std::string pull(const Endpoint& endpoint,const MetricsReport& report,const std::atomic<bool>* cancellation){
        reportPath(report.path);if(report.size>32*1024*1024)throw std::runtime_error("Rapport OVR supérieur à 32 Mo. Enregistrez une session plus courte.");
        auto destination=cache/(L"incoming-"+std::to_wstring(++fileSequence)+L".csv");
        try{auto job=pullRemoteFile(report.path,destination.wstring());job.timeoutMs=10000;execute(endpoint,job,cancellation);auto data=bytes(destination,32*1024*1024);std::error_code ec;std::filesystem::remove(destination,ec);return data;}catch(...){std::error_code ec;std::filesystem::remove(destination,ec);throw;}
    }
    void publish(const std::string& raw,const std::string& name,const std::string& source,bool growing,const std::atomic<bool>* cancellation){
        auto parsed=std::make_shared<MetricsCsv>(parseMetricsCsv(raw,growing,cancellation));parsed->name=name;if(cancellation&&*cancellation)throw std::runtime_error("Lecture annulée.");
        update([&](auto& s){s.data=parsed;++s.dataRevision;s.sourceDevice=source;s.error.clear();});
    }
    void manifest(const std::string& status){
        J files=J::array();for(const auto& [remote,path]:savedPaths)files.push_back({{"file",path.filename().u8string()},{"source",remote}});
        J value={{"schema","cyvr.metrics-session"},{"version",1},{"device",recordDevice.serial},{"status",status},{"files",files},{"csvChangedByQuestDev",ownedCsv}};atomicWrite(recordFolder/L"session.json",value.dump(2));
    }
    void poll(const Endpoint& endpoint,const std::atomic<bool>* cancellation,bool final=false){
        auto reports=scan(endpoint,cancellation);update([&](auto& s){s.reports=reports;});bool selectedLoaded=false;
        bool initial=false;{std::lock_guard lock(mutex);initial=!recording&&!state.data&&selectedRemote.empty()&&!reports.empty();}
        if(initial){auto raw=pull(endpoint,reports.front(),cancellation);publish(raw,reports.front().name,endpoint.serial,true,cancellation);selectedRemote=reports.front().path;selectedSignature=signature(reports.front());selectedLoaded=true;update([](auto& s){s.status="Dernier rapport OVR ouvert · sélectionnez un rapport ou démarrez un enregistrement.";});}
        for(const auto& report:reports){
            auto sig=signature(report);bool changed=recording&&(!baseline.count(report.path)||baseline[report.path]!=sig);bool save=changed&&(!savedSignatures.count(report.path)||savedSignatures[report.path]!=sig||final);bool read=following&&report.path==selectedRemote&&(sig!=selectedSignature||final);
            if(!save&&!read)continue;auto raw=pull(endpoint,report,cancellation);
            if(save){if(!savedPaths.count(report.path)){if(savedPaths.size()>=64)throw std::runtime_error("Limite de 64 rapports par enregistrement.");savedPaths[report.path]=recordFolder/(std::to_string(savedPaths.size()+1)+"-"+safeName(report.name));}atomicWrite(savedPaths[report.path],raw);savedSamples[report.path]=!parseMetricsCsv(raw,!final,cancellation).times.empty();savedSignatures[report.path]=sig;}
            if(read||(save&&autoSelectRecording&&!selectedLoaded)){publish(raw,report.name,endpoint.serial,!final,cancellation);selectedRemote=report.path;selectedSignature=sig;following=!final;selectedLoaded=true;autoSelectRecording=false;}
        }
        if(recording){manifest(final?"finished":"recording");update([&](auto& s){s.savedFiles=savedPaths.size();s.status=s.savedFiles?"Enregistrement sur PC · "+std::to_string(s.savedFiles)+" rapport(s)":"En attente des rapports OVR · "+std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-recordStarted).count())+" s";});if(!final&&savedPaths.empty()&&std::chrono::steady_clock::now()>=nextDiagnostic)diagnose(endpoint,cancellation);else if(!savedPaths.empty())update([](auto& s){s.diagnostic.clear();});}
        if(!recording&&!selectedLoaded&&reports.empty())update([](auto& s){s.status="Aucun rapport CSV disponible. OVR crée ses rapports pendant une session VR enregistrée.";});
        update([&](auto& s){s.remote=selectedRemote;s.follow=following;});
    }
    void start(const Endpoint& endpoint,const std::filesystem::path& destination,const std::shared_ptr<Command>& command){
        if(recording)throw std::runtime_error("Un enregistrement de métriques est déjà actif.");if(!endpoint.online)throw std::runtime_error("Aucun Quest connecté.");if(destination.empty())throw std::runtime_error("Un dossier de sauvegarde est requis.");
        std::filesystem::create_directories(destination.parent_path());if(!std::filesystem::create_directory(destination))throw std::runtime_error("Le dossier de session existe déjà.");
        recordFolder=destination;recordDevice=endpoint;baseline.clear();savedSignatures.clear();savedPaths.clear();savedSamples.clear();
        auto initial=csv(endpoint,"read_state",command->cancel.get());bool active=initial.at("configuration").at("csv").get<bool>();
        for(const auto& report:scan(endpoint,command->cancel.get()))baseline[report.path]=signature(report);
        ownedCsv=!active;recording=true;selectedRemote.clear();selectedSignature.clear();following=true;autoSelectRecording=true;
        recordStarted=std::chrono::steady_clock::now();nextDiagnostic=recordStarted+std::chrono::seconds(5);
        try{if(ownedCsv)csv(endpoint,"csv_start",command->cancel.get());manifest("recording");}
        catch(...){try{finish(false);}catch(...){}throw;}
        update([&](auto& s){s.data.reset();++s.dataRevision;s.recording=true;s.ownsCsv=ownedCsv;s.recordingFolder=recordFolder;s.recordingId=destination.filename().u8string();s.savedFiles=0;s.remote.clear();s.follow=true;s.error.clear();s.diagnostic.clear();s.status="Enregistrement activé · attente du premier rapport CSV OVR.";});
    }
    MetricsResult finish(bool cancelled){
        std::string failure;auto destination=recordFolder;
        if(ownedCsv)try{csv(recordDevice,"csv_stop",nullptr);}catch(const std::exception& e){failure="Impossible de restaurer le mode CSV sur le casque d’origine : "+std::string(e.what());}
        try{poll(recordDevice,nullptr,true);}catch(const std::exception& e){if(!failure.empty())failure+='\n';failure+=e.what();}
        try{manifest(cancelled?"cancelled":failure.empty()?"finished":"interrupted");}catch(const std::exception& e){if(!failure.empty())failure+='\n';failure+=e.what();}
        recording=false;following=false;ownedCsv=false;autoSelectRecording=false;update([&](auto& s){s.recording=false;s.follow=false;s.ownsCsv=false;s.savedFiles=savedPaths.size();s.error=failure;s.status=s.savedFiles?"Enregistrement terminé · "+std::to_string(s.savedFiles)+" rapport(s) sauvegardé(s).":"Enregistrement terminé · aucun CSV produit par OVR pendant cette session.";});
        bool hasSamples=std::any_of(savedSamples.begin(),savedSamples.end(),[](const auto& item){return item.second;});
        MetricsResult result;result.ok=!cancelled&&failure.empty();result.error=cancelled?"Enregistrement annulé.":failure;result.data={{"file",destination.u8string()},{"mimeType","inode/directory"},{"filesSaved",savedPaths.size()},{"hasSamples",hasSamples}};return result;
    }
    void loop(){
        for(;;){
            std::shared_ptr<Command> command;Endpoint endpoint;bool exiting=false;
            {std::unique_lock lock(mutex);wake.wait_for(lock,std::chrono::milliseconds(100),[&]{return quit||!queue.empty()||!same(desired,observed);});endpoint=desired;exiting=quit;if(!queue.empty()){command=queue.front();queue.pop_front();}}
            if(exiting){if(recording){auto result=finish(true);if(timed){auto done=timed;{std::lock_guard lock(mutex);timed.reset();}complete(done,std::move(result));}}if(command)complete(command,{false,J::object(),"Application fermée."});break;}
            if(!same(endpoint,observed)){
                if(recording){auto result=finish(true);if(timed){auto done=timed;{std::lock_guard lock(mutex);timed.reset();}complete(done,std::move(result));}}
                following=false;selectedRemote.clear();selectedSignature.clear();observed=endpoint;update([](auto& s){s.follow=false;s.remote.clear();s.reports.clear();s.reportDirectories.clear();s.diagnostic.clear();});
            }
            if(command){
                {std::lock_guard lock(mutex);interrupt=command->cancel;state.busy=true;state.error.clear();}
                MetricsResult result;result.ok=true;bool defer=false;
                try{
                    if(*command->cancel)throw std::runtime_error("Opération annulée.");auto action=command->action;
                    if(action=="open_csv"){auto data=std::make_shared<MetricsCsv>(readMetricsCsv(command->path,false,command->cancel.get()));if(*command->cancel)throw std::runtime_error("Lecture annulée.");following=false;autoSelectRecording=false;selectedRemote.clear();update([&](auto& s){s.data=data;++s.dataRevision;s.sourceDevice="fichier local";s.follow=false;s.remote.clear();s.status="Session CSV ouverte.";});}
                    else if(action=="export_csv"){std::shared_ptr<const MetricsCsv> data;{std::lock_guard lock(mutex);data=state.data;}if(!data)throw std::runtime_error("Aucune session à exporter.");atomicWrite(command->path,data->raw);result.data={{"file",command->path.u8string()},{"mimeType","text/csv"}};}
                    else if(action=="pause"){following=false;update([](auto& s){s.follow=false;});}
                    else if(action=="refresh_reports"){if(demo)throw std::runtime_error("Démonstration sans ADB.");if(!endpoint.online)throw std::runtime_error("Aucun Quest connecté.");poll(endpoint,command->cancel.get());diagnose(endpoint,command->cancel.get());}
                    else if(action=="open_report"||action=="follow_report"){
                        if(!endpoint.online)throw std::runtime_error("Aucun Quest connecté.");auto path=reportPath(command->remote);auto reports=scan(endpoint,command->cancel.get());auto it=std::find_if(reports.begin(),reports.end(),[&](const auto& report){return report.path==path;});if(it==reports.end())throw std::runtime_error("Rapport CSV absent.");auto raw=pull(endpoint,*it,command->cancel.get());publish(raw,it->name,endpoint.serial,action=="follow_report",command->cancel.get());selectedRemote=path;selectedSignature=signature(*it);following=action=="follow_report";autoSelectRecording=false;update([&](auto& s){s.reports=reports;s.remote=path;s.follow=following;s.status=following?"Suivi du fichier CSV OVR · cadence selon les écritures du casque.":"Rapport OVR ouvert.";});
                    }else if(action=="start_record"||action=="record_timed"){
                        if(action=="record_timed"&&(command->seconds<1||command->seconds>300))throw std::runtime_error("Durée d’enregistrement : 1 à 300 secondes.");
                        if(demo)throw std::runtime_error("Démonstration sans enregistrement.");start(endpoint,command->path,command);result.data={{"recording",true},{"folder",recordFolder.u8string()}};
                        if(action=="record_timed"){{std::lock_guard lock(mutex);timed=command;}endTime=std::chrono::steady_clock::now()+std::chrono::seconds(command->seconds);defer=true;}
                    }else if(action=="stop_record"){if(!recording)throw std::runtime_error("Aucun enregistrement à arrêter.");result=finish(false);if(timed){auto done=timed;{std::lock_guard lock(mutex);timed.reset();}complete(done,result);}}
                    else throw std::runtime_error("Action de métriques inconnue.");
                }catch(const std::exception& e){result.ok=false;result.error=e.what();update([&](auto& s){s.error=result.error;});}
                {std::lock_guard lock(mutex);state.busy=false;interrupt.reset();}if(!defer){if(result.ok&&result.data.empty()){std::lock_guard lock(mutex);result.data=summary(state);}complete(command,std::move(result));}
            }
            if(timed&&(*timed->cancel||std::chrono::steady_clock::now()>=endTime)){
                auto done=timed;auto result=finish(*done->cancel);{std::lock_guard lock(mutex);timed.reset();}complete(done,std::move(result));
            }
            if((following||recording)&&endpoint.online&&std::chrono::steady_clock::now()>=nextPoll){
                auto cancellation=std::make_shared<std::atomic<bool>>(false);{std::lock_guard lock(mutex);interrupt=cancellation;}
                try{poll(recording?recordDevice:endpoint,cancellation.get());}
                catch(const std::exception& e){update([&](auto& s){s.error=e.what();});}
                {std::lock_guard lock(mutex);interrupt.reset();}nextPoll=std::chrono::steady_clock::now()+std::chrono::seconds(1);
            }
        }
        std::deque<std::shared_ptr<Command>> rest;{std::lock_guard lock(mutex);rest.swap(queue);}for(auto& command:rest)complete(command,{false,J::object(),"Application fermée."});
    }
};
MetricsCapture::MetricsCapture(std::filesystem::path cache,bool demo):impl_(std::make_unique<Impl>(std::move(cache),demo)){}
MetricsCapture::~MetricsCapture()=default;
void MetricsCapture::device(std::wstring adb,std::string serial,uint64_t session,bool online){std::lock_guard lock(impl_->mutex);Endpoint value{std::move(adb),std::move(serial),session,online};if(!same(value,impl_->desired)){impl_->desired=std::move(value);impl_->state.online=online;impl_->state.deviceSession=session;if(impl_->interrupt)*impl_->interrupt=true;impl_->wake.notify_all();}}
MetricsState MetricsCapture::state()const{std::lock_guard lock(impl_->mutex);return impl_->state;}
MetricsTicket MetricsCapture::request(const std::string& action,const std::filesystem::path& path,const std::string& remote,int seconds){auto command=std::make_shared<Impl::Command>();command->action=action;command->path=path;command->remote=remote;command->seconds=seconds;auto future=command->promise.get_future().share();{std::lock_guard lock(impl_->mutex);if(impl_->queue.size()>=16)throw std::runtime_error("Trop de commandes de métriques en attente.");command->id=++impl_->sequence;impl_->pending[command->id]=command;impl_->queue.push_back(command);}impl_->wake.notify_all();return {command->id,std::move(future)};}
void MetricsCapture::cancel(uint64_t ticket){std::lock_guard lock(impl_->mutex);auto it=impl_->pending.find(ticket);if(it!=impl_->pending.end())if(auto command=it->second.lock())*command->cancel=true;impl_->wake.notify_all();}
J MetricsCapture::describe()const{return summary(state());}
J MetricsCapture::query(const J& p)const{
    auto s=state();auto result=summary(s);result["samples"]=J::array();if(!s.data)return result;
    auto requested=p.value("metrics",std::vector<std::string>{});if(requested.size()>16)throw std::runtime_error("16 métriques maximum par requête.");std::vector<size_t> indices;if(requested.empty()){for(size_t i=0;i<std::min<size_t>(s.data->columns.size(),8);++i)indices.push_back(i);}else for(const auto& name:requested){auto at=std::find(s.data->columns.begin(),s.data->columns.end(),name);if(at==s.data->columns.end())throw std::runtime_error("Métrique inconnue : "+name);indices.push_back(size_t(at-s.data->columns.begin()));}
    auto start=p.value("start",0.0),end=p.value("end",s.data->times.empty()?0:s.data->times.back());int limit=std::clamp(p.value("limit",100),1,2000);auto begin=std::lower_bound(s.data->times.begin(),s.data->times.end(),start);size_t n=0,totalBytes=0;for(auto it=begin;it!=s.data->times.end()&&*it<=end;++it){if(n++>=size_t(limit))break;size_t row=size_t(it-s.data->times.begin());J values=J::object();for(size_t column:indices){auto value=s.data->value(row,column);values[s.data->columns[column]]=std::isfinite(value)?J(value):J(nullptr);}J sample={{"time",*it},{"values",values}};auto size=sample.dump().size();if(totalBytes+size>200000)break;totalBytes+=size;result["samples"].push_back(std::move(sample));}result["returned"]=result["samples"].size();return result;
}
}
