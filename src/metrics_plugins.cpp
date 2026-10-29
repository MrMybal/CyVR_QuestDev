#include "quest/metrics_plugins.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <atomic>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace quest {
using J=PluginJson;
const std::vector<MetricDescriptor>& hudMetrics(){
    static const std::vector<MetricDescriptor> values={
        {"average_frame_rate","Average FPS","Frame / GPU"},{"app_gpu_time_microseconds","App GPU Time","Frame / GPU"},
        {"timewarp_gpu_time_microseconds","Timewarp GPU Time","Frame / GPU"},{"cpu_utilization_percentage","CPU Utilization","CPU"},
        {"gpu_utilization_percentage","GPU Utilization","Frame / GPU"},{"cpu_frequency_MHz","CPU Frequency","CPU"},
        {"gpu_frequency_MHz","GPU Frequency","Frame / GPU"},{"battery_level_percentage","Battery Level","Device"},
        {"battery_temperature_celcius","Battery Temperature","Device"},{"available_memory_MB","Available Memory","Memory"},
        {"stale_frame_count","Stale Frame Count","Frame / GPU"},{"display_refresh_rate","Display Refresh Rate","Frame / GPU"},
        {"percent_time_shading_frags","Percent of Time Shading Fragments","Frame / GPU"},{"percent_time_shading_verts","Percent of Time Shading Vertices","Frame / GPU"},
        {"app_pss_MB","Used Memory (MB) [PSS]","Memory"},{"cpu_level","CPU Level","CPU"},{"gpu_level","GPU Level","Frame / GPU"},
        {"early_frame_count","Early Frame Count","Frame / GPU"},{"foveation_level","Foveation Level","Frame / GPU"},
        {"eye_buffer_width","Eye Buffer Width","Frame / GPU"},{"eye_buffer_height","Eye Buffer Height","Frame / GPU"},
        {"spacewarped_frames_per_second","SpaceWarp FPS","Frame / GPU"},{"app_vss_MB","Used Memory (MB) [VSS]","Memory"},
        {"app_rss_MB","Used Memory (MB) [RSS]","Memory"},{"app_uss_MB","Used Memory (MB) [USS]","Memory"},
        {"app_private_dirty_MB","Private Dirty Memory (MB)","Memory"},{"app_private_clean_MB","Private Clean Memory (MB)","Memory"},
        {"stale_frames_consecutive","Max Consecutive Stale Frames","Frame / GPU"},{"vrshell_average_frame_rate","VrShell Average Frame Rate","Frame / GPU"},
        {"vrshell_and_guardian_gpu_time_microseconds","VrShell + Boundary GPU Time","Frame / GPU"}};
    return values;
}
bool configurablePlugin(const Plugin& p){return p.error.empty()&&(p.controller=="ovr_metrics"||p.controller=="cyinsight_ovr");}
J pluginDefaults(const Plugin& p){
    J j={{"graphs",{"average_frame_rate","app_gpu_time_microseconds"}},{"stats",J::array()}};
    if(p.controller=="ovr_metrics")j.update({{"overlay",true},{"headlocked",true},{"graphsVisible",true},{"statsVisible",false},{"pitch",8.0},{"yaw",-12.0},{"scale",1.0},{"distance",2.0},{"csv",false}});
    else if(p.controller=="cyinsight_ovr")j.update({{"size","small"},{"corner","upper_left"},{"package",""},{"seconds",0.3}});
    else return J::object();return j;
}
J pluginSettingsSchema(const Plugin& p){
    J properties=J::object(),ids=J::array();for(const auto& m:hudMetrics())ids.push_back(m.id);
    properties["graphs"]={{"type","array"},{"items",{{"type","string"},{"enum",ids}}},{"uniqueItems",true},{"maxItems",p.controller=="cyinsight_ovr"?6:30}};
    properties["stats"]=properties["graphs"];properties["stats"]["maxItems"]=p.controller=="cyinsight_ovr"?8:30;
    auto boolean=[&](const char* key){properties[key]={{"type","boolean"}};};
    auto number=[&](const char* key,double minimum,double maximum){properties[key]={{"type","number"},{"minimum",minimum},{"maximum",maximum}};};
    if(p.controller=="ovr_metrics"){boolean("overlay");boolean("headlocked");boolean("graphsVisible");boolean("statsVisible");boolean("csv");number("pitch",-90,90);number("yaw",-180,180);number("scale",1,3);number("distance",.1,2);}
    else if(p.controller=="cyinsight_ovr"){
        properties["size"]={{"type","string"},{"enum",{"small","medium"}}};
        properties["corner"]={{"type","string"},{"enum",{"upper_left","upper_center","upper_right","center_left","center","center_right","lower_left","lower_center","lower_right"}}};
        properties["package"]={{"type","string"},{"maxLength",200}};number("seconds",.05,2);
    }
    return {{"type","object"},{"properties",properties},{"additionalProperties",false}};
}
J pluginControlActions(const Plugin& p){
    J actions=J::array();auto add=[&](const char* id,const char* label,bool settings=false){actions.push_back({{"id",id},{"label",label},{"settingsSchema",settings?pluginSettingsSchema(p):J{{"type","object"},{"additionalProperties",false}}}});};
    if(!configurablePlugin(p))return actions;
    add("read_state","Lire l’état");add("apply","Appliquer le HUD",true);add("hide",p.controller=="cyinsight_ovr"?"Arrêter / restaurer le HUD":"Masquer le HUD");
    if(p.controller=="ovr_metrics"){add("csv_start","Démarrer le CSV");add("csv_stop","Arrêter le CSV");}
    else {add("catalog","Actualiser les métriques disponibles");add("status","État du profileur");add("enable_details","Activer le profilage détaillé",true);add("capture","Capturer les tuiles GPU",true);}
    return actions;
}
J validatePluginSettings(const Plugin& p,const J& values){
    if(!configurablePlugin(p)||!values.is_object()||values.dump().size()>16384)throw std::runtime_error("Configuration de plugin invalide.");
    auto result=pluginDefaults(p),schema=pluginSettingsSchema(p)["properties"];
    for(auto it=values.begin();it!=values.end();++it){
        if(!schema.contains(it.key()))throw std::runtime_error("Réglage inconnu : "+it.key());const auto& s=schema[it.key()];const auto& v=it.value();auto type=s["type"].get<std::string>();
        if(type=="boolean"&&!v.is_boolean())throw std::runtime_error("Booléen attendu : "+it.key());
        if(type=="number"&&(!v.is_number()||!std::isfinite(v.get<double>())||v.get<double>()<s["minimum"].get<double>()||v.get<double>()>s["maximum"].get<double>()))throw std::runtime_error("Nombre hors limites : "+it.key());
        if(type=="string"&&(!v.is_string()||v.get<std::string>().size()>200))throw std::runtime_error("Texte invalide : "+it.key());
        if(s.contains("enum")&&std::find(s["enum"].begin(),s["enum"].end(),v)==s["enum"].end())throw std::runtime_error("Choix invalide : "+it.key());
        if(type=="array"){
            if(!v.is_array()||v.size()>s["maxItems"].get<size_t>())throw std::runtime_error("Trop de métriques : "+it.key());std::set<std::string> unique;
            for(const auto& item:v){if(!item.is_string()||!unique.insert(item.get<std::string>()).second||std::none_of(hudMetrics().begin(),hudMetrics().end(),[&](const MetricDescriptor& m){return item==m.id;}))throw std::runtime_error("Métrique inconnue ou dupliquée.");}
        }
        result[it.key()]=v;
    }
    if(p.controller=="cyinsight_ovr"&&!result["package"].get<std::string>().empty()&&!validPackage(result["package"].get<std::string>()))throw std::runtime_error("Package cible invalide.");
    return result;
}
namespace {
std::string broadcast(const std::string& action){return "am broadcast -n com.oculus.ovrmonitormetricsservice/.SettingsBroadcastReceiver -a com.oculus.ovrmonitormetricsservice."+action;}
std::string base64(const std::string& value){
    static const char* digits="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;unsigned buffer=0;int bits=0;
    for(unsigned char c:value){buffer=(buffer<<8)|c;bits+=8;while(bits>=6){bits-=6;out+=digits[(buffer>>bits)&63];}}
    if(bits)out+=digits[(buffer<<(6-bits))&63];while(out.size()%4)out+='=';return out;
}
bool selected(const J& j,const char* id){return std::find(j.begin(),j.end(),id)!=j.end();}
std::string readMeta(){return "stamp=$(date +%s%3N); case \"$stamp\" in *[!0-9]*|'') echo 'Device clock unavailable'; exit 1;; esac; echo CYVR_METRICS_SINCE=$stamp; "+broadcast("LOG_STATE")+" >/dev/null || exit $?; sleep 0.3; logcat -d -v brief -t 1000 | grep -E '^[VDIWEF]/SettingsBroadcastReceiver[.]Info[.]'";}
J parseMeta(const std::string& text){
    std::istringstream lines(text);std::string line;uint64_t since=0;std::map<uint64_t,std::string> chunks;
    const std::regex prefix(R"(^[VDIWEF]/SettingsBroadcastReceiver\.Info\.(\d+)\(\s*\d+\): ?(.*)$)");
    while(std::getline(lines,line)){
        line=trim(line);if(line.rfind("CYVR_METRICS_SINCE=",0)==0){try{since=std::stoull(line.substr(19));}catch(...){}continue;}
        std::smatch match;if(!std::regex_match(line,match,prefix))continue;uint64_t stamp=0;try{stamp=std::stoull(match[1]);}catch(...){continue;}
        if(!since||stamp<since)continue;auto part=match[2].str();if(part=="Start::"||part=="End::")continue;
        auto& value=chunks[stamp];if(value.size()+part.size()>128*1024)throw std::runtime_error("État OVR trop volumineux.");value+=part;
    }
    for(auto it=chunks.rbegin();it!=chunks.rend();++it){auto j=J::parse(it->second,nullptr,false);if(j.is_object()&&j.contains("overlayEnabled")&&j.contains("overlayScale")&&j.contains("overlayDistance")&&j.contains("overlayPitch")&&j.contains("overlayYaw")&&j.contains("overlayHeadLocked")&&j.contains("statsEnabled")&&j.contains("graphEnabled")&&j.contains("metrics"))return j;}
    throw std::runtime_error("Aucun état OVR complet et récent reçu. La commande n’est pas considérée comme vérifiée.");
}
const J* metric(const J& state,const char* title){if(!state.contains("metrics")||!state["metrics"].is_array())return nullptr;for(const auto& item:state["metrics"]){auto name=item.value("name",std::string());if(name==title||name.rfind(std::string(title)+" (",0)==0)return &item;}return nullptr;}
J normalizeMeta(const J& state){
    J values={{"overlay",state.at("overlayEnabled")},{"headlocked",state.at("overlayHeadLocked")},{"graphsVisible",state.at("graphEnabled")},{"statsVisible",state.at("statsEnabled")},{"pitch",state.at("overlayPitch")},{"yaw",state.at("overlayYaw")},{"scale",state.at("overlayScale")},{"distance",state.at("overlayDistance")},{"graphs",J::array()},{"stats",J::array()}};
    if(state.contains("recordingMetricsEnabled"))values["csv"]=state["recordingMetricsEnabled"];
    for(const auto& m:hudMetrics())if(auto* item=metric(state,m.title)){if(item->value("graphEnabled",false))values["graphs"].push_back(m.id);if(item->value("statEnabled",false))values["stats"].push_back(m.id);}return values;
}
}
JobRequest metricsPluginJob(const Plugin& p,const std::string& action,const J& input){
    if(!p.enabled||!p.error.empty()||!configurablePlugin(p))throw std::runtime_error("Plugin configurable désactivé ou invalide.");
    auto actions=pluginControlActions(p);if(std::none_of(actions.begin(),actions.end(),[&](const J& a){return a["id"]==action;}))throw std::runtime_error("Action de plugin inconnue.");
    if(action!="apply"&&action!="capture"&&action!="enable_details"&&!input.empty())throw std::runtime_error("Cette action ne prend pas de réglages.");
    auto settings=validatePluginSettings(p,input);JobRequest job;job.label=p.name+" · "+action;job.timeoutMs=60000;
    if(p.controller=="cyinsight_ovr"){
        const std::map<std::string,std::string> ops={{"read_state","snapshot"},{"catalog","hud_catalog"},{"status","status"},{"apply","overlay_meta_on"},{"hide","overlay_meta_off"},{"capture","capture"},{"enable_details","enable_details"}};
        J request={{"operation",ops.at(action)}};
        if(action=="apply"){if(settings["graphs"].empty()&&settings["stats"].empty())throw std::runtime_error("Choisissez un graphique ou une valeur.");for(auto key:{"size","corner","graphs","stats"})request[key]=settings[key];}
        if(action=="capture"||action=="enable_details"){auto target=settings["package"].get<std::string>();if(!validPackage(target)||target==p.package)throw std::runtime_error("Sélectionnez le package exact du jeu cible.");request["package"]=target;if(action=="capture")request["seconds"]=settings["seconds"];}
        job.steps={{{"shell","CLASSPATH=/data/local/tmp/cyinsightovr-bridge.jar app_process /system/bin org.cyinsights.ovr.ShellClient --request "+base64(request.dump())},{}}};
    }else{
        std::string script;
        if(action=="apply"){
            auto num=[&](const char* key){return settings.at(key).dump();};
            script=broadcast("ENABLE_OVERLAY")+" --ez headlocked "+num("headlocked")+" --ef pitch "+num("pitch")+" --ef yaw "+num("yaw")+" --ef scale "+num("scale")+" --ef distance "+num("distance")+" >/dev/null || exit $?; ";
            for(const auto& m:hudMetrics())for(const auto* type:{"graphs","stats"}){bool on=selected(settings[type],m.id);auto command=std::string(type)=="graphs"?(on?"ENABLE_GRAPH":"DISABLE_GRAPH"):(on?"ENABLE_STAT":"DISABLE_STAT");script+=broadcast(command)+" --es stat "+shellQuote(m.id)+" >/dev/null || exit $?; ";}
            script+=broadcast(settings["graphsVisible"].get<bool>()?"ENABLE_GRAPH":"DISABLE_GRAPH")+" >/dev/null || exit $?; ";
            script+=broadcast(settings["statsVisible"].get<bool>()?"ENABLE_STATS":"DISABLE_STATS")+" >/dev/null || exit $?; ";
            script+=broadcast(settings["csv"].get<bool>()?"ENABLE_CSV":"DISABLE_CSV")+" >/dev/null || exit $?; ";
            if(!settings["overlay"].get<bool>())script+=broadcast("DISABLE_OVERLAY")+" >/dev/null || exit $?; ";
        }else if(action!="read_state")script=broadcast(action=="hide"?"DISABLE_OVERLAY":action=="csv_start"?"ENABLE_CSV":"DISABLE_CSV")+" >/dev/null || exit $?; ";
        script+=readMeta();job.timeoutMs=120000;job.steps={{{"shell",script},{}}};
    }return job;
}
J metricsPluginResult(const Plugin& p,const std::string& action,const std::string& output,const J& requested,J* capture){
    if(output.size()>2*1024*1024)throw std::runtime_error("Réponse du plugin trop volumineuse.");
    if(p.controller=="cyinsight_ovr"){
        std::istringstream lines(output);std::string line;J result;
        while(std::getline(lines,line)){auto j=J::parse(trim(line),nullptr,false);if(j.is_object())result=std::move(j);}
        if(!result.is_object())throw std::runtime_error("Réponse du collecteur CyInsightOVR absente. Initialisez le collecteur avec son JAR.");
        if(result.contains("error"))throw std::runtime_error(result.at("error").is_string()?result.at("error").get<std::string>():"Erreur du collecteur.");
        if(result.contains("accepted")&&!result.at("accepted").get<bool>())throw std::runtime_error("Le collecteur a refusé la commande : "+result.value("status",std::string()));
        if(action=="apply"&&!result.value("settings_verified",false))throw std::runtime_error("Les réglages du HUD n’ont pas été vérifiés par le collecteur.");
        if(action=="read_state"&&!result.value("bridge",false))throw std::runtime_error("État CyInsightOVR invalide.");
        if(action=="bootstrap"&&!result.contains("android_boot_id"))throw std::runtime_error("Le collecteur n’a pas confirmé sa disponibilité.");
        if(action=="capture"&&(!result.value("accepted",false)||!result.contains("capture_id")))throw std::runtime_error("La capture GPU n’a pas été acceptée.");
        // Geometry is retrieved as .cyovr files through the file browser, not
        // repeated in every assistant context update.
        result.erase("history");if(result.contains("capture")&&result["capture"].is_object()){
            const auto& value=result["capture"];if(capture&&value.value("schema",std::string())=="cyinsightovr.capture"&&value.value("version",0)==1&&value.contains("surfaces")&&value["surfaces"].is_array()&&!value.value("capture_id",std::string()).empty())*capture=value;
            result["capture"].erase("surfaces");
        }
        return result;
    }
    auto state=parseMeta(output),values=normalizeMeta(state);J result={{"state",state},{"configuration",values},{"settingsVerified",false},{"visibleInHeadset","requires_visual_confirmation"}};
    if(action=="apply"){
        auto expected=validatePluginSettings(p,requested);
        for(auto key:{"overlay","headlocked","graphsVisible","statsVisible","pitch","yaw","scale","distance","csv"}){
            if(!values.contains(key))throw std::runtime_error(std::string("État OVR sans ")+key+" : vérification impossible.");
            bool equal=expected[key].is_number()?std::abs(expected[key].get<double>()-values[key].get<double>())<.05:expected[key]==values[key];if(!equal)throw std::runtime_error(std::string("OVR n’a pas appliqué le réglage : ")+key);
        }
        for(const auto& m:hudMetrics()){
            auto* item=metric(state,m.title);bool graph=selected(expected["graphs"],m.id),stat=selected(expected["stats"],m.id);
            if(!item){if(graph||stat)throw std::runtime_error(std::string("Métrique indisponible : ")+m.title);continue;}
            if(item->value("graphEnabled",false)!=graph||item->value("statEnabled",false)!=stat)throw std::runtime_error(std::string("Sélection OVR non appliquée : ")+m.title);
        }
        result["settingsVerified"]=true;
    }else if(action=="hide"||action=="csv_start"||action=="csv_stop"){
        auto key=action=="hide"?"overlay":"csv";bool target=action=="csv_start";if(!values.contains(key)||values[key]!=target)throw std::runtime_error("OVR n’a pas confirmé le changement.");result["settingsVerified"]=true;
    }return result;
}
JobRequest cyInsightBootstrap(const std::wstring& jar){
    std::error_code ec;if(!std::filesystem::is_regular_file(jar,ec)||std::filesystem::path(jar).extension()!=L".jar"||std::filesystem::file_size(jar,ec)>16*1024*1024)throw std::runtime_error("Sélectionnez le JAR officiel du compagnon CyInsightOVR (16 Mo maximum).");
    static std::atomic<uint64_t> sequence{0};auto incoming="/data/local/tmp/cyinsightovr-questdev-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++sequence)+".jar";
    // Pair entirely on the Quest. The capability never enters host output or arguments.
    std::string script="set -e; identity=$(cmd package list packages -U org.cyinsights.ovr); uid=$(printf '%s\\n' \"$identity\" | sed -n 's/^package:org\\.cyinsights\\.ovr uid:\\([0-9][0-9]*\\)$/\\1/p'); case \"$uid\" in ''|*[!0-9]*) echo 'CyInsightOVR package not installed'; exit 1;; esac; test \"$uid\" -ge 10000; ";
    // Do not replace code mapped by an existing helper or restart it implicitly.
    script+="if ! pidof cyinsightovr-bridge >/dev/null; then chmod 600 "+shellQuote(incoming)+"; mv -f "+shellQuote(incoming)+" /data/local/tmp/cyinsightovr-bridge.jar; umask 077; CLASSPATH=/data/local/tmp/cyinsightovr-bridge.jar nohup app_process /system/bin --nice-name=cyinsightovr-bridge org.cyinsights.ovr.ShellBridge \"$uid\" >/data/local/tmp/cyinsightovr-bridge.log 2>&1 </dev/null & sleep 1; fi; ";
    script+="test -s /data/local/tmp/cyinsightovr-connection.json; cat /data/local/tmp/cyinsightovr-connection.json | run-as org.cyinsights.ovr sh -c 'umask 077; mkdir -p files; cat > files/developer-bridge.json'; CLASSPATH=/data/local/tmp/cyinsightovr-bridge.jar app_process /system/bin org.cyinsights.ovr.ShellClient --request "+base64(J{{"operation","companion_capabilities"}}.dump());
    JobRequest job;job.label="CyInsightOVR · initialiser le collecteur";job.timeoutMs=30000;
    job.steps={{{"push",narrow(jar),incoming},{}},{{"shell",script},{}}};
    job.cleanupSteps={{{"shell","rm -f "+shellQuote(incoming)},{}}};return job;
}
}
