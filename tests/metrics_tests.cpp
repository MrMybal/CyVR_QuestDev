// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/metrics_capture.hpp"
#include "quest/metrics_plugins.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <thread>
using namespace quest;
static int checks=0;
void check(bool condition,const char* name){++checks;if(!condition)throw std::runtime_error(name);}
template<class F>void rejects(F function,const char* name){bool rejected=false;try{function();}catch(...){rejected=true;}check(rejected,name);}
MetricsResult wait(MetricsTicket ticket,int seconds=12){if(ticket.result.wait_for(std::chrono::seconds(seconds))!=std::future_status::ready)throw std::runtime_error("Metrics worker timeout");return ticket.result.get();}
void write(const std::filesystem::path& path,const std::string& data){std::ofstream output(path,std::ios::binary);output<<data;if(!output)throw std::runtime_error("Fixture write failed");}
int main(int argc,char** argv){try{
    auto csv=parseMetricsCsv("\xef\xbb\xbfTime Stamp,average_frame_rate,custom,empty\r\n1000,90,-3,\r\n1500,80,2,N/A\r\n");
    check(csv.columns.size()==3&&csv.times.size()==2&&csv.times[1]==.5,"BOM, CRLF and millisecond time basis");
    check(csv.value(0,1)==-3&&std::isnan(csv.value(0,2))&&std::isnan(csv.value(1,2)),"negative custom metric preserved and unavailable not zero");
    auto quoted=parseMetricsCsv("\"Time Stamp\",\"custom,metric\",\"say \"\"yes\"\"\"\n0,4,\"notes\nline\"\n1,5,6");check(quoted.columns[0]=="custom,metric"&&quoted.columns[1]=="say \"yes\""&&quoted.times.size()==2,"RFC CSV quotes and embedded newline");
    std::string many="timestamp";for(int i=0;i<80;++i)many+=",metric_"+std::to_string(i);many+="\n0";for(int i=0;i<80;++i)many+=","+std::to_string(i);many+='\n';check(parseMetricsCsv(many).columns.size()==80,"dynamic catalog beyond HUD managed metrics");
    rejects([]{parseMetricsCsv("time,a\n0,1\n");},"timestamp column required");
    rejects([]{parseMetricsCsv("Time Stamp,a,A\n0,1,2\n");},"duplicate names rejected");
    rejects([]{parseMetricsCsv("Time Stamp,a\n0,1,2\n");},"row shape rejected");
    rejects([]{parseMetricsCsv("Time Stamp,a\n10,1\n5,2\n");},"clock reset rejected");
    rejects([]{parseMetricsCsv("Time Stamp,a\nnan,1\n");},"non-finite time rejected");
    rejects([]{parseMetricsCsv("Time Stamp,a\n0,ab\"c\n");},"stray quotes rejected");
    rejects([]{parseMetricsCsv("Time Stamp,a\n0,\"unfinished");},"partial quote rejected on completed file");
    auto growing=parseMetricsCsv("Time Stamp,a\n0,2\n100,4",true);check(growing.times.size()==1&&growing.incompleteRows==1&&growing.raw.back()=='\n',"in-flight final row waits for newline");
    auto invalid=parseMetricsCsv("Time Stamp,a\n0,nan\n1,inf\n2,12x\n3,0\n");check(std::isnan(invalid.value(0,0))&&std::isnan(invalid.value(1,0))&&std::isnan(invalid.value(2,0))&&invalid.value(3,0)==0,"zero and missing observations stay distinct");
    auto spike=parseMetricsCsv("timestamp,a\n0,1\n1,1000\n2,2\n3,\n4,5\n");auto envelope=metricsPlot(spike,0,0,1,1);check(envelope.count==4&&envelope.high==1000&&envelope.buckets[0].missing,"pixel envelope preserves spikes and missing segments");
    check(std::abs(envelope.average-252)<1e-9,"mean over measured samples only");
    auto range=metricsPlot(csv,0,.5,.6,100);check(range.count==1&&range.low==80,"shared range selects correct source samples");
    check(metricsUnit("app_gpu_time_microseconds")=="µs"&&metricsUnit("custom").empty(),"units only from explicit names");
    std::atomic<bool> cancelled{true};rejects([&]{parseMetricsCsv(many,false,&cancelled);},"CSV parser supports cancellation");
    auto demo=parseMetricsCsv(demoMetricsCsv());check(demo.times.size()==360&&demo.columns.size()==7,"demo uses same parser");
    auto base=std::filesystem::current_path()/("metrics-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));std::filesystem::create_directories(base);
    auto source=base/L"source.csv";write(source,demoMetricsCsv());
    {
        MetricsCapture monitor(base/L"cache");auto opened=wait(monitor.request("open_csv",source));check(opened.ok&&monitor.state().data->times.size()==360,"native CSV loading on worker");
        auto result=monitor.query({{"metrics",{"average_frame_rate","custom_metric"}},{"limit",3}});check(result["samples"].size()==3&&result["columns"].size()==7,"SDK samples bounded with dynamic catalogue");
        rejects([&]{monitor.query({{"metrics",{"not_a_metric"}}});},"unknown query metric rejected");
        auto destination=base/L"export.csv";check(wait(monitor.request("export_csv",destination)).ok&&readMetricsCsv(destination).times.size()==360,"CSV export reopens with same samples");
        write(base/L"bad.csv","x,y\n1,2\n");check(!wait(monitor.request("open_csv",base/L"bad.csv")).ok&&monitor.state().data->times.size()==360,"failed import preserves current session");
        auto ticket=monitor.request("open_csv",source);monitor.cancel(ticket.id);check(!wait(ticket).ok,"worker cancellation acknowledges completion");
    }
    if(argc==2){
        auto fake=std::filesystem::absolute(argv[1]);auto reports=base/L"device";std::filesystem::create_directory(reports);SetEnvironmentVariableW(L"QUEST_FAKE_METRICS_ROOT",reports.c_str());
        {
            MetricsCapture monitor(base/L"remote-cache");monitor.device(fake.wstring(),"FAKE-QUEST",1,true);
            auto recording=wait(monitor.request("record_timed",base/L"recording","",2));check(recording.ok&&recording.data["filesSaved"].get<int>()==1&&recording.data["hasSamples"]==true,"timed recording collects actual fake-ADB CSV writes");
            check(!std::filesystem::exists(reports/L"enabled")&&!monitor.state().recording,"owned CSV mode restored on stop");
            check(monitor.state().data&&monitor.state().data->columns.size()==3&&monitor.state().data->times.size()>=2,"growing report reaches live plots");
            auto state=monitor.state();auto session=base/L"recording/1-quest.csv";check(std::filesystem::is_regular_file(session)&&readMetricsCsv(session).times.size()>=2,"recorded file survives completion and reopens");
            write(reports/L"enabled","already on");check(wait(monitor.request("record_timed",base/L"preserve","",1)).ok&&std::filesystem::exists(reports/L"enabled"),"pre-existing CSV mode preserved");
            std::filesystem::remove(reports/L"enabled");auto ticket=monitor.request("record_timed",base/L"cancelled","",30);auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!monitor.state().recording&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(10));monitor.cancel(ticket.id);check(!wait(ticket).ok&&!std::filesystem::exists(reports/L"enabled"),"cancel waits for restoration on original device");
            check(!wait(monitor.request("open_report",{},"/sdcard/not-ovr.csv")).ok,"remote reports scoped to OVR directory");
            check(!wait(monitor.request("record_timed",base/L"bad-duration","",0)).ok&&!std::filesystem::exists(reports/L"enabled"),"invalid recording duration cannot enable CSV");
            auto switched=monitor.request("record_timed",base/L"device-switch","",30);until=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!monitor.state().recording&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(10));
            check(monitor.state().recording,"recording started before device switch");monitor.device(fake.wstring(),"ANOTHER-QUEST",2,true);
            check(!wait(switched).ok&&!std::filesystem::exists(reports/L"enabled"),"switch restores CSV on original device, never new selection");
        }
        MetricsTicket closing;
        {MetricsCapture monitor(base/L"close-cache");monitor.device(fake.wstring(),"FAKE-QUEST",3,true);closing=monitor.request("record_timed",base/L"closing","",30);auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!monitor.state().recording&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(10));check(monitor.state().recording,"recording started before shutdown");}
        check(!wait(closing).ok&&!std::filesystem::exists(reports/L"enabled"),"normal shutdown waits for CSV restoration and acknowledges cancellation");
        auto empty=base/L"empty-device";std::filesystem::create_directory(empty);write(empty/L"suppress_samples","fixture");SetEnvironmentVariableW(L"QUEST_FAKE_METRICS_ROOT",empty.c_str());
        {MetricsCapture monitor(base/L"empty-cache");monitor.device(fake.wstring(),"FAKE-QUEST",1,true);auto result=wait(monitor.request("record_timed",base/L"empty-session","",1));check(result.ok&&result.data["filesSaved"]==1&&result.data["hasSamples"]==false,"header-only CSV is a saved report, never measured samples");}
        SetEnvironmentVariableW(L"QUEST_FAKE_METRICS_ROOT",nullptr);
    }
    if(argc==4&&std::string(argv[1])=="--live"){
        auto adb=std::filesystem::absolute(argv[2]).wstring();std::string serial=argv[3];auto plugin=parsePlugin("[plugin]\napi=1\nid=meta.ovr-metrics\nname=OVR Metrics\npackage=com.oculus.ovrmonitormetricsservice\ncontroller=ovr_metrics");
        auto read=[&]{auto job=metricsPluginJob(plugin,"read_state");std::string output;for(auto& step:job.steps){auto args=step.args;args.insert(args.begin(),{"-s",serial});auto result=runProcess(adb,args,10000);check(result.code==0&&!result.timedOut,"live OVR state command");output+=result.output;}return metricsPluginResult(plugin,"read_state",output).at("configuration").at("csv").get<bool>();};
        bool before=read();MetricsCapture monitor(base/L"live-cache");monitor.device(adb,serial,1,true);auto result=wait(monitor.request("record_timed",base/L"live-session","",2),45);check(result.ok,result.error.c_str());check(read()==before,"live original CSV mode restored");std::cout<<"Live recording: "<<result.data.dump()<<"\n";
    }
    std::cout<<checks<<" CSV/metrics checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<"\n";return 1;}}
