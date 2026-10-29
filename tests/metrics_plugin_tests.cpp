#include "quest/metrics_plugins.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace quest;
using J=PluginJson;
static int count=0;
void check(bool ok,const char* name){++count;if(!ok)throw std::runtime_error(name);}
template<class F> void rejected(F f,const char* name){bool failed=false;try{f();}catch(...){failed=true;}check(failed,name);}
Plugin plugin(const std::string& controller){return parsePlugin("[plugin]\napi=1\nid=test."+controller+"\nname=Test\ncontroller="+controller+"\npackage="+(controller=="ovr_metrics"?"com.oculus.ovrmonitormetricsservice":"org.cyinsights.ovr"));}
J state(){
    J metrics=J::array();for(const auto& m:hudMetrics())metrics.push_back({{"name",m.title},{"graphEnabled",std::string(m.id)=="average_frame_rate"||std::string(m.id)=="app_gpu_time_microseconds"},{"statEnabled",false}});
    return {{"overlayEnabled",true},{"overlayHeadLocked",true},{"overlayPitch",8.0},{"overlayYaw",-12.0},{"overlayScale",1.0},{"overlayDistance",2.0},{"recordingMetricsEnabled",false},{"statsEnabled",false},{"graphEnabled",true},{"metrics",metrics}};
}
std::string logs(const J& s,uint64_t stamp=1234567890123ULL){auto json=s.dump();size_t split=json.size()/2;auto prefix="I/SettingsBroadcastReceiver.Info."+std::to_string(stamp)+"( 4321): ";return "CYVR_METRICS_SINCE=1234567890123\n"+prefix+"Start::\n"+prefix+json.substr(0,split)+"\n"+prefix+json.substr(split)+"\n"+prefix+"End::\n";}
int main(int argc,char** argv){try{
    auto ovr=plugin("ovr_metrics"),cy=plugin("cyinsight_ovr");check(ovr.error.empty()&&cy.error.empty(),"known controllers parse");
    check(!parsePlugin("[plugin]\napi=1\nid=x\nname=x\ncontroller=ovr_metrics\npackage=com.test.other").error.empty(),"controller restricted to actual package");
    check(!parsePlugin("[plugin]\napi=1junk\nid=x\nname=x\npackage=com.test.other").error.empty(),"partial API rejected");
    check(hudMetrics().size()==30,"documented managed catalog");
    rejected([&]{validatePluginSettings(ovr,{{"scale",.3}});},"clamped scale cannot be requested");
    rejected([&]{validatePluginSettings(ovr,{{"distance",3}});},"distance bound");
    rejected([&]{validatePluginSettings(ovr,{{"headlocked","false"}});},"bool type enforced");
    rejected([&]{validatePluginSettings(ovr,{{"pitch",nullptr}});},"numeric null rejected");
    rejected([&]{validatePluginSettings(ovr,{{"shell","reboot"}});},"unknown setting rejected");
    rejected([&]{validatePluginSettings(cy,{{"package","com.test.x; reboot"}});},"target shell injection rejected");
    rejected([&]{validatePluginSettings(cy,{{"corner","somewhere"}});},"position enum enforced");
    rejected([&]{validatePluginSettings(cy,{{"graphs",{"average_frame_rate","average_frame_rate"}}});},"duplicate metric rejected");
    rejected([&]{validatePluginSettings(cy,{{"stats",{"unknown_stat"}}});},"unknown metric rejected");
    J tooMany=J::array();for(int i=0;i<7;++i)tooMany.push_back(hudMetrics()[i].id);rejected([&]{validatePluginSettings(cy,{{"graphs",tooMany}});},"graph limit six");
    rejected([&]{metricsPluginJob(cy,"apply",{{"graphs",J::array()},{"stats",J::array()}});},"empty companion HUD rejected");
    rejected([&]{metricsPluginJob(cy,"capture",{{"package","org.cyinsights.ovr"}});},"self capture rejected");
    rejected([&]{metricsPluginJob(cy,"reboot");},"unlisted operation rejected");
    auto command=metricsPluginJob(ovr,"apply").steps[0].args[1];check(command.find("--ef distance 2.0")!=std::string::npos&&command.find("logcat -c")==std::string::npos,"documented parameters without clearing logs");
    auto initial=metricsPluginResult(ovr,"read_state",logs(state()));check(initial["configuration"]["graphs"].size()==2&&!initial["configuration"]["csv"].get<bool>(),"fresh chunked state assembled and normalized");
    check(metricsPluginResult(ovr,"apply",logs(state()))["settingsVerified"].get<bool>(),"apply verified against actual state");
    auto clamp=state();clamp["overlayScale"]=1.4;rejected([&]{metricsPluginResult(ovr,"apply",logs(clamp));},"service clamping never reported as success");
    rejected([&]{metricsPluginResult(ovr,"read_state",logs(state(),1234567890122ULL));},"stale log rejected");
    rejected([&]{metricsPluginResult(ovr,"read_state","Broadcast completed: result=0");},"broadcast success alone insufficient");
    auto unsupported=state();unsupported["metrics"]=J::array();rejected([&]{metricsPluginResult(ovr,"apply",logs(unsupported));},"missing selected metric rejected");
    auto h=state();h["overlayEnabled"]=false;check(metricsPluginResult(ovr,"hide",logs(h))["settingsVerified"].get<bool>(),"hide confirmed");
    rejected([&]{metricsPluginResult(ovr,"csv_start",logs(state()));},"CSV rejection detected");
    rejected([&]{metricsPluginResult(cy,"apply","{\"error\":\"Permission denied\"}\n");},"companion semantic error even with ADB zero");
    rejected([&]{metricsPluginResult(cy,"apply","{\"accepted\":true}\n");},"accepted HUD requires verification");
    auto capture=metricsPluginResult(cy,"capture","{\"accepted\":true,\"capture_id\":\"test-123\"}\n");check(capture["capture_id"]=="test-123"&&!capture.contains("completed"),"accepted capture stays pending");
    auto snap=metricsPluginResult(cy,"read_state","{\"bridge\":true,\"capture\":{\"capture_id\":\"other\",\"surfaces\":[]},\"history\":[]}\n");check(snap["capture"]["capture_id"]=="other"&&!snap.contains("history"),"snapshot identity preserved");
    J complete;auto compact=metricsPluginResult(cy,"read_state","{\"bridge\":true,\"capture\":{\"schema\":\"cyinsightovr.capture\",\"version\":1,\"capture_id\":\"completed-id\",\"surfaces\":[]}}\n",J::object(),&complete);check(complete["surfaces"].is_array()&&!compact["capture"].contains("surfaces"),"full capture retained separately from compact AI context");
    auto hiddenStats=state();hiddenStats["metrics"][0]["statEnabled"]=true;hiddenStats["statsEnabled"]=false;auto copied=metricsPluginResult(ovr,"read_state",logs(hiddenStats))["configuration"];check(!copied["statsVisible"].get<bool>()&&copied["stats"].size()==1,"global stat visibility survives configuration copy");
    if(argc==3&&std::string(argv[1])=="--fake-adb"){
        for(auto& p:{ovr,cy}){auto job=metricsPluginJob(p,"read_state");auto args=std::vector<std::string>{"-s","FAKE-QUEST"};args.insert(args.end(),job.steps[0].args.begin(),job.steps[0].args.end());auto r=runProcess(widen(argv[2]),args,5000);check(r.code==0,"fake ADB completed");check(metricsPluginResult(p,"read_state",r.output).is_object(),"real process to plugin result parser");}
    }
    if(argc==5&&std::string(argv[1])=="--bootstrap"){
        auto job=cyInsightBootstrap(widen(argv[4]));std::string output;bool ok=true;
        for(const auto& step:job.steps){auto args=std::vector<std::string>{"-s",argv[3]};args.insert(args.end(),step.args.begin(),step.args.end());auto r=runProcess(widen(argv[2]),args,job.timeoutMs);output+=r.output;if(r.code){ok=false;break;}}
        for(const auto& step:job.cleanupSteps){auto args=std::vector<std::string>{"-s",argv[3]};args.insert(args.end(),step.args.begin(),step.args.end());runProcess(widen(argv[2]),args,job.timeoutMs);}
        if(!ok)throw std::runtime_error(output);auto result=metricsPluginResult(cy,"bootstrap",output);check(result.contains("android_boot_id"),"real Quest bootstrap and private pairing");std::cout<<"Real collector ready; protocol="<<result.value("protocol",std::string())<<"\n";
    }
    if(argc==4&&std::string(argv[1])=="--live-read"){
        for(auto& p:{ovr,cy}){auto job=metricsPluginJob(p,"read_state");auto args=std::vector<std::string>{"-s",argv[3]};args.insert(args.end(),job.steps[0].args.begin(),job.steps[0].args.end());auto r=runProcess(widen(argv[2]),args,job.timeoutMs);if(r.code)throw std::runtime_error(r.output);auto result=metricsPluginResult(p,"read_state",r.output);std::ofstream out(std::filesystem::path("captures")/(p.controller+"-live-state.json"));out<<result.dump(2);check(bool(out),"real device state saved");std::cout<<p.controller<<": real device state read\n";}
    }
    std::cout<<count<<" plugin checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<"\n";return 1;}}
