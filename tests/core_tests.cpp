// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/backend.hpp"
#include "quest/plugins.hpp"
#include <filesystem>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace quest;
static int checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static const LogEntry& message(const LogStore& s,const std::string& text){for(const auto& e:s.entries())if(e.message==text)return e;throw std::runtime_error("missing log");}
int main(int argc,char** argv) {
    try {
        auto devices=parseDevices("List of devices attached\nABC device model:Quest_3 transport_id:1\nXYZ unauthorized\nOFF offline\n* daemon started successfully *\n");
        check(devices.size()==3 && devices[0].model=="Quest 3" && devices[1].state=="unauthorized","device states");
        auto packages=parsePackages("package:com.test.alpha uid:10001\npackage:com.test.beta uid:10001\nError: denied\n");
        check(packages.size()==2 && packages[0].uid==10001,"packages and shared uid");
        check(validPackage("com.test.alpha")&&!validPackage("com.test;reboot")&&!validPackage("-com.test"),"remote shell package validation");
        check(!parseLog("--------- beginning of main")&&!parseLog("error: device offline")&&!parseLog("nan 1 1 I Tag: x"),"invalid log lines");
        auto e=parseLog("1700000000.123456  501  502 E Unreal   : text: with colon\r");
        check(e && e->tag=="Unreal" && e->message=="text: with colon" && e->pid==501 && e->level=='E',"epoch parser");
        check(e&&rawLog(*e)=="1700000000.123456  501  502 E Unreal   : text: with colon","RAW retains original Logcat spacing, tag and timestamp");
        auto a=*e,b=*e;a.id=1;b.id=2;a.epoch=2;b.epoch=1;a.pid=2;b.pid=10;a.level='V';b.level='E';a.package="com.a";b.package="com.z";a.tag="Z";b.tag="A";a.message="beta";b.message="alpha";
        check(!logLess(a,b,{{LogColumn::time,false}})&&logLess(a,b,{{LogColumn::time,true}}),"timestamp ascending and descending sorting");
        check(logLess(a,b,{{LogColumn::pid,false}})&&logLess(a,b,{{LogColumn::level,false}}),"PID and severity sorted numerically");
        check(logLess(a,b,{{LogColumn::application,false}})&&!logLess(a,b,{{LogColumn::tag,false}})&&logLess(a,b,{{LogColumn::message,true}}),"application, tag and message sorting");
        a.tag=b.tag;check(logLess(a,b,{{LogColumn::tag,false},{LogColumn::pid,false}}),"multi-column sorting uses secondary keys");
        check(logLess(a,b,{{LogColumn::tag,false}})&&!logLess(b,a,{{LogColumn::tag,false}}),"equal sort keys retain deterministic capture order");
        check(logFollow(true,500,500,500,false,2),"incoming rows preserve follow at previous bottom");
        check(!logFollow(true,500,420,500,false,2),"manual upward drag suspends follow");
        check(logFollow(false,420,500,500,false,2),"manual return to bottom automatically resumes follow");
        check(!logFollow(true,500,500,500,true,2),"upward wheel pauses even before scroll position changes");
        check(logFollow(true,500,300,300,false,2),"retention pruning cannot disable following");
        check(!logFollow(false,500,500,500,false,2),"explicitly disabled follow stays disabled without scrolling");
        LogStore store;store.setPackages(packages);store.snapshot({{501,10001,"com.test.alpha"},{601,10001,"com.test.beta:remote"}},100);
        store.ingest("100.100 501 502 I SameTag: alpha");store.ingest("100.110 601 602 E SameTag: beta");
        check(message(store,"alpha").package=="com.test.alpha" && message(store,"beta").package=="com.test.beta","separate same tag and shared uid");
        store.ingest("100.200 800 800 I am_proc_died: [0,501,com.test.alpha,0,0]");
        store.ingest("100.300 501 502 I SameTag: unowned-gap");
        store.ingest("100.400 800 800 I am_proc_start: [0,501,10001,com.test.beta:worker,service,test]");
        store.ingest("100.500 501 502 W SameTag: reused-pid");
        check(message(store,"alpha").package=="com.test.alpha" && message(store,"reused-pid").package=="com.test.beta" && message(store,"unowned-gap").package.empty(),"PID lifetime reuse");
        store.ingest("100.800 701 702 I SameTag: before-start-arrives");
        store.ingest("100.700 800 800 I am_proc_start: [0,701,10001,com.test.alpha:renderer,service,test]");
        check(message(store,"before-start-arrives").package=="com.test.alpha","out of order lifecycle event");
        store.ingest("99.500 501 502 I SameTag: old-history");
        check(message(store,"old-history").package.empty(),"no retroactive snapshot guess");
        store.snapshot({{501,10001,"com.test.beta:worker"},{701,10001,"com.test.alpha:renderer"}},101);
        check(message(store,"alpha").package=="com.test.alpha","snapshots preserve historical identity");
        LogFilter f;f.packages.insert("com.test.alpha");f.minimum=2;
        check(f.matches(message(store,"alpha"))&&!f.matches(message(store,"beta")),"package filter");f.exclude="alpha";check(!f.matches(message(store,"alpha")),"exclude filter");f.exclude.clear();f.text="sameTAG";f.text=lower(f.text);check(f.matches(message(store,"alpha")),"case insensitive search");
        store.ingest("102.100 701 702 E Tag: quote \" tab\t UTF8 é");
        auto out=exportLogs({&store.entries().back()},true);check(out.find("\\\"")!=std::string::npos&&out.find("\\t")!=std::string::npos&&out.find("UTF8 é")!=std::string::npos,"JSON escaping and unicode");
        LogStore bounded(3);for(int i=0;i<5;++i)bounded.ingest("110.1 1 1 I Tag: "+std::to_string(i));check(bounded.entries().size()==3&&bounded.discarded()==2,"bounded ring");
        auto revision=bounded.revision();bounded.setLimits(6,64*1024*1024);check(bounded.entries().size()==3&&bounded.entries().front().id==3&&bounded.discarded()==2&&bounded.revision()>revision,"growing retention preserves records and IDs");
        bounded.ingest("110.2 1 1 I Tag: next");bounded.ingest("110.3 1 1 I Tag: last");check(bounded.entries().size()==5&&bounded.capacity()==6,"new retention takes effect without restarting capture");
        bounded.setLimits(2,64*1024*1024);check(bounded.entries().size()==2&&bounded.entries().front().id==6&&bounded.discarded()==5,"shrinking retention immediately removes oldest records");
        bounded.setLimits(2,1024);bounded.ingest("110.4 1 1 I Tag: "+std::string(5000,'x'));check(bounded.bytes()<=1024&&bounded.entries().empty()&&bounded.byteLimit()==1024,"runtime memory limit applies to oversized new records");
        bounded.setLimits(10,65536);bounded.setPackages(packages);bounded.snapshot({{1,10001,"com.test.alpha"}},111);bounded.ingest("111.1 1 1 I Tag: after-resize");check(bounded.entries().back().package=="com.test.alpha"&&bounded.entries().back().id>7,"resizing preserves attribution and increasing record IDs");
        LogStore byteBound(100,1024);for(int i=0;i<10;++i)byteBound.ingest("111.1 1 1 I Tag: "+std::string(300,'x'));check(byteBound.bytes()<=1024&&byteBound.discarded()>0,"bounded bytes");
        byteBound.setPackages(packages);byteBound.snapshot({{1,10001,"com.test.alpha"}},110);check(byteBound.bytes()<=1024,"reattribution keeps memory bound");
        store.clear();store.ingest("103.1 701 702 I Tag: identity-after-clear");check(store.entries().size()==1&&store.entries()[0].package=="com.test.alpha","clear keeps attribution");store.reset();check(store.entries().empty()&&store.processes().empty(),"device reset");
        if(argc>1) {
            auto exe=widen(argv[1]);auto quoted=runProcess(exe,{"--echo","path with spaces","C:\\trailing\\","a\"b","é"});
            check(quoted.code==0&&quoted.output=="path with spaces\nC:\\trailing\\\na\"b\né\n","process argument quoting");
            auto timeout=runProcess(exe,{"--hang"},120);check(timeout.timedOut&&timeout.code!=0,"process cancellation");
            auto unrelated=std::filesystem::current_path()/(L"handle-isolation-"+std::to_wstring(GetCurrentProcessId())+L".tmp");SECURITY_ATTRIBUTES inheritable{sizeof(inheritable),nullptr,TRUE};
            auto file=CreateFileW(unrelated.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&inheritable,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);check(file!=INVALID_HANDLE_VALUE,"fresh inheritable handle fixture");
            std::atomic<bool> ready{false},cancelHold{false};std::thread held([&]{runProcess(exe,{"--hold"},10000,&cancelHold,[&](const std::string& chunk){if(chunk.find("ready")!=std::string::npos)ready=true;});});
            auto handleDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!ready&&std::chrono::steady_clock::now()<handleDeadline)std::this_thread::sleep_for(std::chrono::milliseconds(5));
            CloseHandle(file);bool isolated=DeleteFileW(unrelated.c_str())!=FALSE;cancelHold=true;held.join();std::error_code cleanupError;std::filesystem::remove(unrelated,cleanupError);check(ready&&isolated,"background ADB cannot inherit and lock unrelated local files");
            std::string parseError;auto arguments=parseCommand("pull \"/sdcard/a file.txt\" \"C:\\My Files\\a.txt\"",parseError);
            check(parseError.empty()&&arguments.size()==3&&arguments[2]=="C:\\My Files\\a.txt","terminal argument parser keeps paths");
            parseCommand("shell 'unfinished",parseError);check(!parseError.empty(),"terminal rejects unfinished quote");
            check(shellQuote("a'b")=="'a'\\''b'","Android shell quoting");
            auto plugin=parsePlugin("[plugin]\napi=1\nid=test.plugin\nname=Test\npackage=com.test.alpha\n[link]\nlocal_port=8787\nremote_port=9797\ndirection=forward\n[action.inspect]\nlabel=Inspect\nshell=dumpsys package {package}\n");
            check(plugin.error.empty()&&plugin.actions.size()==1,"plugin manifest parsing");
            auto link=pluginLink(plugin);check(link.steps[0].args==std::vector<std::string>({"forward","--no-rebind","tcp:8787","tcp:9797"}),"plugin bridge does not overwrite existing mapping");
            check(pluginAction(plugin,plugin.actions[0]).steps[0].args[1]=="dumpsys package 'com.test.alpha'","plugin action substitution");
            check(!parsePlugin("[plugin]\napi=99\nid=x\nname=x\npackage=com.test.alpha").error.empty(),"plugin rejects unsupported API");
            check(!parsePlugin("[plugin]\napi=1\nid=x\nname=x\npackage=com.test.alpha\n[link]\nlocal_port=70000\nremote_port=1").error.empty(),"plugin port validation");
            auto binaryPath=std::filesystem::current_path()/L"questdev-binary-test.bin";
            auto binary=runProcess(exe,{"-s","FAKE-QUEST","exec-out","screencap","-p"},2000,nullptr,{},binaryPath.wstring());
            std::ifstream binaryFile(binaryPath,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(binaryFile)),{});binaryFile.close();std::filesystem::remove(binaryPath);
            check(binary.code==0&&bytes.size()==7&&bytes[3]=='\0'&&binary.output.find("binary diagnostic")!=std::string::npos,"binary stdout stays separate from stderr");
            {
                Backend jobs(exe);jobs.select("FAKE-QUEST");auto hang=jobs.submit({"long",{{{"shell","job-hang"},{}}},10000,true,{{{"shell","job-stream"},{}}}});auto stream=jobs.submit({"short",{{{"shell","job-stream"},{}}},1000,true});auto failure=jobs.submit({"failure",{{{"shell","job-fail"},{}}},1000,true});
                bool streamDone=false,hangDone=false,failed=false,output=false,cleaned=false;auto until=std::chrono::steady_clock::now()+std::chrono::seconds(4);jobs.cancel(hang);
                while(std::chrono::steady_clock::now()<until&&(!streamDone||!hangDone||!failed)){
                    for(const auto& event:jobs.poll()){
                        if(event.job==stream&&event.kind==BackendEvent::JobOutput&&event.text.find("chunk-")!=std::string::npos)output=true;
                        if(event.job==hang&&event.kind==BackendEvent::JobOutput&&event.text.find("chunk-3")!=std::string::npos)cleaned=true;
                        if(event.kind==BackendEvent::JobDone){if(event.job==stream)streamDone=event.ok;if(event.job==hang)hangDone=event.cancelled&&!event.ok;if(event.job==failure)failed=!event.ok&&event.code==17;}
                    }std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                check(streamDone&&output,"ADB jobs stream without blocking a concurrent command");check(hangDone&&cleaned,"ADB cancellation acknowledges process exit and cleanup");check(failed,"ADB exit status is preserved");
            }
            Backend backend(exe);uint64_t sid=backend.select("FAKE-QUEST");LogStore captured;bool seenDevices=false,operation=false;
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
            while(std::chrono::steady_clock::now()<deadline) {
                for(auto& event:backend.poll()) {
                    if(event.kind==BackendEvent::Devices)seenDevices=!event.devices.empty();
                    if(event.session!=sid)continue;
                    if(event.kind==BackendEvent::Packages)captured.setPackages(event.packages);
                    if(event.kind==BackendEvent::Processes)captured.snapshot(event.processes,event.epoch);
                    if(event.kind==BackendEvent::Lines)for(const auto& line:event.lines)captured.ingest(line);
                    if(event.kind==BackendEvent::Operation)operation=event.ok;
                }
                if(captured.entries().size()>=5)break;std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            check(seenDevices&&captured.entries().size()>=5,"backend starts capture");
            check(message(captured,"app A ready").package=="com.test.alpha"&&message(captured,"app B ready").package=="com.test.beta"&&message(captured,"restarted renderer").package=="com.test.alpha","end to end application routing");
            backend.action("launch","com.test.alpha");deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            while(std::chrono::steady_clock::now()<deadline&&!operation){for(const auto& event:backend.poll())if(event.kind==BackendEvent::Operation)operation=event.ok;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
            check(operation,"launch activity");auto newSid=backend.select("OTHER-DEVICE");check(newSid!=sid,"device session isolation");
            backend.select("");backend.action("connect","192.168.1.10:5555");bool connected=false;deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            while(std::chrono::steady_clock::now()<deadline&&!connected){for(const auto& event:backend.poll())if(event.kind==BackendEvent::Operation&&event.text.find("connected to")!=std::string::npos)connected=event.ok;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
            check(connected,"wifi connect without selected device");
            SetEnvironmentVariableW(L"QUEST_FAKE_LOGCAT_EXIT",L"1");
            {
                Backend retry(exe);auto retrySid=retry.select("FAKE-QUEST");int retries=0,lines=0;deadline=std::chrono::steady_clock::now()+std::chrono::seconds(7);
                while(std::chrono::steady_clock::now()<deadline && (retries<2||lines<10)) {
                    for(const auto& event:retry.poll())if(event.session==retrySid){if(event.kind==BackendEvent::Status && event.ok)++retries;if(event.kind==BackendEvent::Lines)lines+=int(event.lines.size());}
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                check(retries>=2&&lines>=10,"automatic capture recovery after client exit");
            }
            SetEnvironmentVariableW(L"QUEST_FAKE_LOGCAT_EXIT",nullptr);
        }
        std::cout<<checks<<" checks passed\n";return 0;
    } catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<"\n";return 1;}
}
