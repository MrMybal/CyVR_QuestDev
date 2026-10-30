// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/mirroring.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <thread>
using namespace quest;
static int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F function,const char* label){bool bad=false;try{function();}catch(...){bad=true;}check(bad,label);}
MirrorResult wait(MirrorTicket ticket,int seconds=12){check(ticket.result.wait_for(std::chrono::seconds(seconds))==std::future_status::ready,"native mirroring acknowledgement timeout");return ticket.result.get();}
std::string read(const std::filesystem::path& file){std::ifstream in(file,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});}
int main(int argc,char** argv){try{
    MirrorOptions options;auto args=mirrorArguments("TEST-QUEST","Quest video",options);auto has=[&](const std::string& text){return std::find(args.begin(),args.end(),text)!=args.end();};
    check(has("--serial=TEST-QUEST")&&has("--max-fps=60")&&has("--video-codec=h264"),"explicit target and video settings");
    check(has("--audio-dup")&&has("--no-control")&&has("--no-clipboard-autosync"),"headset audio preserved and remote input disabled by default");
    check(!has("--kill-adb-on-close")&&!has("--stay-awake")&&!has("--tcpip"),"no global ADB or power/network changes");
    options.audio=0;args=mirrorArguments("TEST","title",options);check(has("--no-audio")&&!has("--audio-dup"),"audio can be disabled");
    options.fullscreen=true;options.alwaysOnTop=true;args=mirrorArguments("TEST","title",options);check(has("--fullscreen")&&has("--always-on-top"),"fullscreen and always-on-top options");
    rejects([&]{auto o=options;o.maxFps=121;mirrorArguments("TEST","t",o);},"FPS limit validated");
    rejects([&]{auto o=options;o.bitRateMbps=0;mirrorArguments("TEST","t",o);},"bitrate validated");
    rejects([&]{mirrorArguments("TEST\n--kill-adb-on-close","t",options);},"device identifier controls rejected");
    rejects([&]{auto o=options;o.cropWidth=100;mirrorArguments("TEST","t",o);},"incomplete crop rejected");
    rejects([&]{auto o=options;o.recording=L"relative.mkv";mirrorArguments("TEST","t",o);},"recording requires absolute MKV path");
    auto root=std::filesystem::current_path()/("mirror-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));std::filesystem::create_directory(root);
    if(argc==3){auto engine=std::filesystem::absolute(argv[1]).wstring(),adb=std::filesystem::absolute(argv[2]).wstring();options=MirrorOptions{};options.recording=root/L"équipe vidéo.mkv";
        wchar_t before[32768]{};GetEnvironmentVariableW(L"ADB",before,32768);
        {Mirroring mirror;check(wait(mirror.start(engine,adb,"FAKE-QUEST",options)).ok,"fake stream starts only after texture and window");auto state=mirror.state();check(state.active&&!state.starting&&state.width==1600&&state.height==900&&state.fps==60,"live dimensions and FPS parsed");check(state.log.find("environment OK")!=std::string::npos,"selected ADB passed in isolated child environment");check(wait(mirror.stop()).ok&&!mirror.state().active,"stop awaits native process exit");check(read(options.recording).find("FINALIZED")!=std::string::npos,"recording finalized before stop acknowledgement");check(wait(mirror.stop()).ok,"idle stop is idempotent");}
        wchar_t after[32768]{};GetEnvironmentVariableW(L"ADB",after,32768);check(std::wstring(before)==after,"parent ADB environment remains intact");
        rejects([&]{mirrorArguments("TEST","t",options);},"existing recording is protected");options.recording.clear();
        SetEnvironmentVariableW(L"QUEST_MIRROR_FAKE_MODE",L"fail");{Mirroring mirror;auto result=wait(mirror.start(engine,adb,"FAKE-QUEST",options));check(!result.ok&&!mirror.state().active&&mirror.state().exitCode==7,"encoder failure is never a running stream");}
        SetEnvironmentVariableW(L"QUEST_MIRROR_FAKE_MODE",L"slow");{Mirroring mirror;auto ticket=mirror.start(engine,adb,"FAKE-QUEST",options);mirror.cancel(ticket.id);check(!wait(ticket).ok,"cancelled start waits for process shutdown");check(!mirror.state().active,"cancel leaves no active native process");}
        SetEnvironmentVariableW(L"QUEST_MIRROR_FAKE_MODE",nullptr);options.recording=root/L"closing.mkv";
        {Mirroring mirror;check(wait(mirror.start(engine,adb,"FAKE-QUEST",options)).ok,"shutdown fixture starts");}check(read(options.recording).find("FINALIZED")!=std::string::npos,"normal app destruction gracefully finalizes recording");
    }
    if(argc==5&&std::string(argv[1])=="--live"){
        Mirroring mirror;options=MirrorOptions{};options.maxSize=1600;options.recording=root/L"quest-live.mkv";auto result=wait(mirror.start(std::filesystem::absolute(argv[2]).wstring(),std::filesystem::absolute(argv[3]).wstring(),argv[4],options),25);check(result.ok,result.error.c_str());std::this_thread::sleep_for(std::chrono::seconds(3));auto state=mirror.state();check(state.width>0&&state.height>0&&state.fps>0,"real Quest texture and FPS received");check(wait(mirror.stop()).ok,"real scrcpy graceful stop");auto data=read(options.recording);check(data.find("V_MPEG4/ISO/AVC")!=std::string::npos,"real MKV contains H264 video");check(data.find("A_AAC")!=std::string::npos,"real MKV contains AAC audio track");std::cout<<"Live: "<<state.width<<'x'<<state.height<<", "<<state.fps<<" FPS, H264/AAC MKV finalized\n";
    }
    std::cout<<checks<<" mirroring checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
