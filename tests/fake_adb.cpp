// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <json.hpp>
using J=nlohmann::json;
static std::filesystem::path metricsRoot(){wchar_t value[32768]{};GetEnvironmentVariableW(L"QUEST_FAKE_METRICS_ROOT",value,32768);return value;}
static void advanceMetrics(const std::filesystem::path& root){if(root.empty()||!std::filesystem::exists(root/L"enabled"))return;auto counter=root/L"counter";int n=0;{std::ifstream in(counter);in>>n;}++n;{std::ofstream out(counter);out<<n;}auto path=root/L"quest.csv";bool first=!std::filesystem::exists(path);std::ofstream out(path,std::ios::app);if(first)out<<"Time Stamp,average_frame_rate,app_gpu_time_microseconds,custom_extra\n";if(!std::filesystem::exists(root/L"suppress_samples"))out<<1000+n*100<<",90,"<<5000+n<<",-3\n";}
static std::string decode(const std::string& text){std::string out;unsigned buffer=0;int bits=0;std::string digits="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";for(char c:text){auto at=digits.find(c);if(at==std::string::npos)break;buffer=(buffer<<6)|unsigned(at);bits+=6;if(bits>=8){bits-=8;out+=char((buffer>>bits)&255);}}return out;}
int wmain(int argc,wchar_t** argv) {
    _setmode(_fileno(stdout),_O_BINARY);
    std::vector<std::string> a;
    for(int i=1;i<argc;++i){int n=WideCharToMultiByte(CP_UTF8,0,argv[i],-1,nullptr,0,nullptr,nullptr);std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,argv[i],-1,s.data(),n,nullptr,nullptr);s.pop_back();a.push_back(s);}
    if(a.empty())return 1;
    if(a[0]=="--echo"){for(size_t i=1;i<a.size();++i)std::cout<<a[i]<<"\n";return 0;}
    if(a[0]=="--hang"){Sleep(10000);return 0;}
    if(a[0]=="devices"){std::cout<<"List of devices attached\nFAKE-QUEST\tdevice product:hollywood model:Quest_3 device:hollywood transport_id:1\n";return 0;}
    if(a[0]=="connect"){std::cout<<"connected to "<<a.back()<<"\n";return 0;}
    if(a.size()<3 || a[0]!="-s" || a[1]!="FAKE-QUEST")return 2;
    if(a[2]=="logcat") {
        double epoch=0;for(size_t i=3;i+1<a.size();++i)if(a[i]=="-T")epoch=std::stod(a[i+1]);
        std::cout<<std::fixed<<std::setprecision(6);
        std::cout<<epoch+.01<<" 501 502 I QuestA: app A ready\n";
        std::cout<<epoch+.02<<" 601 602 W QuestB: app B ready\n";
        std::cout<<epoch+.03<<" 800 800 I am_proc_died: [0,501,com.test.alpha,0,0]\n";
        std::cout<<epoch+.04<<" 800 800 I am_proc_start: [0,701,10001,com.test.alpha:renderer,service,com.test.alpha/.Worker]\n";
        std::cout<<epoch+.05<<" 701 702 E QuestA: restarted renderer\n"<<std::flush;
        if(GetEnvironmentVariableW(L"QUEST_FAKE_LOGCAT_EXIT",nullptr,0))return 0;
        Sleep(60000);return 0;
    }
    if(a[2]=="install"){std::cout<<"Success\n";return 0;}
    if(a[2]=="exec-out"){
        if(a.size()>5&&a[3]=="sh"){
            auto emit=[](std::initializer_list<std::string> fields){for(const auto& field:fields){std::cout<<field;std::cout.put('\0');}};
            if(a.back().find("CapturedMetrics")!=std::string::npos){auto root=metricsRoot();if(root.empty()){emit({"CYVRFILES1","END"});return 0;}advanceMetrics(root);emit({"CYVRFILES1"});for(const auto& entry:std::filesystem::directory_iterator(root))if(entry.is_regular_file()&&entry.path().extension()==L".csv")emit({"f",std::to_string(entry.file_size()),"1700000000",entry.path().filename().u8string(),""});emit({"END"});return 0;}
            if(a.back().find("/denied")!=std::string::npos){emit({"CYVRERROR1","Fixture access denied"});return 0;}
            emit({"CYVRFILES1","d","4096","1700000000","Download","","d","4096","1700000000","Documents","","f","22","1700000000","readme.txt","","f","8","1700000001","équipe's file.txt","","l","10","1700000000","link","../outside","END"});return 0;
        }
        const char data[]={'P','N','G','\0','\r','\n',char(-1)};std::cout.write(data,sizeof(data));std::cerr<<"binary diagnostic\n";return 0;
    }
    if(a[2]=="pull"&&a.size()==5){
        if(a[3].find("/CapturedMetrics/")!=std::string::npos){auto root=metricsRoot();auto source=root/std::filesystem::path(a[3]).filename();if(root.empty()||!std::filesystem::is_regular_file(source))return 1;std::ifstream input(source,std::ios::binary);std::ofstream out(std::filesystem::path(argv[5]),std::ios::binary);out<<input.rdbuf();if(!out)return 1;std::cout<<"1 report pulled\n";return 0;}
        if(a[3]!="/sdcard/readme.txt"){std::cerr<<"Fixture file missing\n";return 1;}std::ofstream out(std::filesystem::path(argv[5]),std::ios::binary);out<<"fixture Quest payload\n";out.close();if(!out)return 1;std::cout<<"1 file pulled\n";return 0;
    }
    if(a[2]=="forward"||a[2]=="reverse"){std::cout<<"8787\n";return 0;}
    if(a[2]!="shell" || a.size()<4)return 3;
    if(a[3].find("CYVR_METRICS_SINCE=")!=std::string::npos){
        auto root=metricsRoot();if(!root.empty()){if(a[3].find("DISABLE_CSV")!=std::string::npos){std::error_code ec;std::filesystem::remove(root/L"enabled",ec);}else if(a[3].find("ENABLE_CSV")!=std::string::npos){std::ofstream out(root/L"enabled");out<<"fixture";}}
        J state={{"overlayEnabled",true},{"overlayHeadLocked",true},{"overlayPitch",8.0},{"overlayYaw",-12.0},{"overlayScale",1.0},{"overlayDistance",2.0},{"recordingMetricsEnabled",false},{"statsEnabled",false},{"graphEnabled",true},{"metrics",J::array({{{"name","Average FPS"},{"graphEnabled",true},{"statEnabled",false}},{{"name","App GPU Time (us)"},{"graphEnabled",true},{"statEnabled",false}}})}};
        state["recordingMetricsEnabled"]=!root.empty()&&std::filesystem::exists(root/L"enabled");
        std::cout<<"CYVR_METRICS_SINCE=1234567890123\nI/SettingsBroadcastReceiver.Info.1234567890123( 1234): "<<state.dump()<<"\n";return 0;
    }
    if(a[3].find("org.cyinsights.ovr.ShellClient --request ")!=std::string::npos){
        auto request=J::parse(decode(a[3].substr(a[3].rfind(' ')+1)));auto op=request.value("operation",std::string());J result={{"protocol","cyinsightovr.bridge/1"},{"android_boot_id","fixture-boot"}};
        if(GetEnvironmentVariableW(L"QUEST_FAKE_PLUGIN_ERROR",nullptr,0))result={{"error","Fixture collector refused"}};
        else if(op=="snapshot")result.update({{"bridge",true},{"status","Fixture system counters"},{"capturing",false},{"metrics",J::array({{{"name","GPU Frequency"},{"value",490.0},{"unit","MHz"}}})},{"capture",{{"schema","cyinsightovr.capture"},{"version",1},{"capture_id","fixture-capture-123"},{"target_package","com.test.alpha"},{"status","unavailable"},{"reason","Synthetic integration fixture"},{"surfaces",J::array()}}}});
        else if(op=="overlay_meta_on")result.update({{"accepted",true},{"settings_verified",true},{"graphs",request.at("graphs")},{"stats",request.at("stats")}});
        else if(op=="capture")result.update({{"accepted",true},{"capture_id","fixture-capture-123"}});
        else if(op=="hud_catalog")result["metrics"]=J::array({{{"id","average_frame_rate"},{"available",true}}});
        else if(op=="status")result["profiler_status"]="Fixture profiler ready";
        else result["accepted"]=true;
        std::cout<<result.dump()<<"\n";return 0;
    }
    if(a[3]=="date") {
        auto now=std::chrono::system_clock::now().time_since_epoch();
        std::cout<<std::fixed<<std::setprecision(6)<<std::chrono::duration<double>(now).count()<<"\n";
    } else if(a[3]=="ps")std::cout<<"  PID UID NAME\n 501 10001 com.test.alpha\n 601 10002 com.test.beta\n 800 1000 system_server\n";
    else if(a[3]=="pm") {bool system=false;for(size_t i=4;i<a.size();++i)if(a[i]=="-s")system=true;if(!system)std::cout<<"package:com.test.alpha uid:10001\npackage:com.test.beta uid:10002\npackage:com.oculus.ovrmonitormetricsservice uid:10003\npackage:org.cyinsights.ovr uid:10004\n";}
    else if(a[3]=="job-stream"){for(int i=0;i<4;++i){std::cout<<"chunk-"<<i<<"\n"<<std::flush;Sleep(20);}}
    else if(a[3]=="job-hang")Sleep(10000);
    else if(a[3]=="job-fail"){std::cout<<"expected error\n";return 17;}
    else if(a[3].rfind("test -f ",0)==0){if(a[3].find("/CapturedMetrics/")!=std::string::npos)return 0;if(a[3].find("/sdcard/readme.txt")==std::string::npos){std::cout<<"Regular file required\n";return 1;}}
    else if(a[3]=="cmd")std::cout<<"com.test.alpha/.MainActivity\n";
    else if(a[3]=="am")std::cout<<"Starting: Intent { cmp=com.test.alpha/.MainActivity }\n";
    else return 4;
    return 0;
}
