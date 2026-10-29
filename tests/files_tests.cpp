// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/files.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
using namespace quest;
static int checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<typename F>static bool rejects(F&& function){try{function();return false;}catch(const std::exception&){return true;}}
static std::string record(std::initializer_list<std::string> fields){std::string bytes;for(const auto& field:fields){bytes+=field;bytes+='\0';}return bytes;}
static std::string read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});}
static void write(const std::filesystem::path& path,const std::string& data){std::ofstream out(path,std::ios::binary);out<<data;if(!out)throw std::runtime_error("fixture write");}
static std::string posix(const std::filesystem::path& path){auto value=narrow(path.generic_wstring());if(value.size()>1&&value[1]==':'){char drive=char(std::tolower(value[0]));return std::string("/")+drive+value.substr(2);}return value;}
static std::filesystem::path windows(const std::string& path){if(path.size()>2&&path[0]=='/'&&path[2]=='/')return std::filesystem::path(widen(std::string(1,path[1])+":"+path.substr(2)));return std::filesystem::path(widen(path));}
int main(int argc,char** argv){try{
    if(argc==4&&std::string(argv[1])=="--live-read"){
        auto destination=std::filesystem::current_path()/L"captures/files-live.bin";auto job=listRemoteFiles("/sdcard",destination.wstring());auto args=std::vector<std::string>{"-s",argv[3]};args.insert(args.end(),job.steps[0].args.begin(),job.steps[0].args.end());auto result=runProcess(widen(argv[2]),args,60000,nullptr,{},destination.wstring());if(result.code)throw std::runtime_error(result.output);auto data=read(destination);std::cout<<"Received "<<data.size()<<" bytes; tail:";for(size_t i=data.size()>80?data.size()-80:0;i<data.size();++i)std::cout<<" "<<std::hex<<unsigned(static_cast<unsigned char>(data[i]));std::cout<<std::dec<<"\n";auto listing=parseFileListing(data);std::cout<<listing.entries.size()<<" real device entries\n";return 0;
    }
    check(remotePath("/sdcard//Download/../Documents/./")=="/sdcard/Documents","path normalization");
    check(remoteParent("/")=="/"&&remoteParent("/sdcard/Download")=="/sdcard","parent navigation");
    check(rejects([]{remotePath("C:\\Windows");})&&rejects([]{remotePath(std::string("/a\0b",4));}),"reject non-Android paths and embedded nulls");
    check(!validRemoteName("..")&&!validRemoteName("a/b")&&!validRemoteName("a\nb")&&validRemoteName("équipe's file.txt"),"new-name validation");
    auto bytes=record({"CYVRFILES1","f","12","1700000000","space ' é\n.txt","","l","3","","link","../outside","d","4096","1700000001","folder","","END"});
    auto listing=parseFileListing(bytes);check(listing.entries.size()==3&&listing.entries[0].type=='d',"directory-first ordering");
    check(listing.entries[1].type=='l'&&listing.entries[1].target=="../outside"&&!listing.entries[1].modified,"symlink target and unknown metadata");
    check(listing.entries[2].name=="space ' é\n.txt"&&listing.entries[2].size==12,"Unicode, quotes, spaces and newline names remain exact");
    check(parseFileListing(record({"CYVRFILES1","END"})).entries.empty(),"valid empty directory");
    check(parseFileListing(record({"CYVRFILES1","TRUNCATED"})).truncated,"explicit limit marker");
    check(rejects([&]{parseFileListing(bytes.substr(0,bytes.size()-1));}),"reject truncated protocol");
    check(rejects([]{parseFileListing(record({"CYVRFILES1","f","10","1","../escape","","END"}));}),"reject path traversal entries");
    check(rejects([]{parseFileListing(record({"CYVRFILES1","f","10","1","same","","f","10","1","same","","END"}));}),"reject duplicated entries");
    check(rejects([]{parseFileListing(record({"CYVRFILES1","f","18446744073709551616","1","big","","END"}));}),"reject numeric overflow");
    check(rejects([]{parseFileListing(record({"CYVRERROR1","Access denied"}));}),"remote permission error protocol");
    check(rejects([]{changeRemoteFile("delete","/sdcard/../");})&&rejects([]{changeRemoteFile("rename","/sdcard/Android","x");}),"protected root mutation rejected");
    check(changeRemoteFile("delete","/sdcard/test").steps[0].args[1].find("rmdir --")!=std::string::npos,"directory deletion requires empty directory");
    if(argc>1){
        auto sh=widen(argv[1]);auto base=std::filesystem::current_path();auto root=base/(L"questdev-files-é-"+std::to_wstring(GetCurrentProcessId()));std::filesystem::create_directory(root);
        check(std::filesystem::weakly_canonical(root).parent_path()==std::filesystem::weakly_canonical(base),"test fixture remains in intended directory");
        auto path=posix(root);auto run=[&](const std::string& script){return runProcess(sh,{"-c","export PATH=/usr/bin:$PATH; "+script},10000);};
        auto source=root/L"source équipe's.txt";write(source,"original payload\n");
        auto actual=run(fileListingScript(path));auto scanned=parseFileListing(actual.output);
        if(actual.code||scanned.entries.size()!=1||scanned.entries[0].name!="source équipe's.txt"||scanned.entries[0].size!=17){std::cerr<<"listing code="<<actual.code<<" entries="<<scanned.entries.size()<<"\n";for(const auto& item:scanned.entries)std::cerr<<"name="<<item.name<<" size="<<(item.size?std::to_string(*item.size):"null")<<"\n";}
        check(actual.code==0&&scanned.entries.size()==1&&scanned.entries[0].name=="source équipe's.txt"&&scanned.entries[0].size==17,"listing script runs on a real POSIX filesystem");
        auto job=listRemoteFiles(path,{});const auto& arguments=job.steps[0].args;std::string adbExec=arguments[1];for(size_t i=2;i<arguments.size();++i)adbExec+=" "+shellQuote(arguments[i]);auto transport=run(adbExec);auto transported=parseFileListing(transport.output);check(transport.code==0&&transported.entries.size()==1&&transported.entries[0].name=="source équipe's.txt","listing survives adb exec-out automatic argument escaping");
        auto mkdir=changeRemoteFile("mkdir",path,"empty folder");check(run(mkdir.steps[0].args[1]).code==0&&std::filesystem::is_directory(root/L"empty folder"),"mkdir executes");
        write(root/L"empty folder/keep.txt","keep");auto remove=changeRemoteFile("delete",path+"/empty folder");
        check(run(remove.steps[0].args[1]).code!=0&&read(root/L"empty folder/keep.txt")=="keep","non-empty directory and contents preserved");
        check(run(changeRemoteFile("delete",path+"/empty folder/keep.txt").steps[0].args[1]).code==0&&run(remove.steps[0].args[1]).code==0,"delete file then empty directory");
        auto rename=changeRemoteFile("rename",path+"/source équipe's.txt","renamed 'é.txt");check(run(rename.steps[0].args[1]).code==0&&read(root/L"renamed 'é.txt")=="original payload\n","rename with apostrophe and Unicode");
        source=root/L"renamed 'é.txt";write(root/L"occupied.txt","keep");
        check(run(changeRemoteFile("rename",posix(source),"occupied.txt").steps[0].args[1]).code!=0&&read(root/L"occupied.txt")=="keep"&&std::filesystem::exists(source),"rename cannot replace another entry");
        check(run(changeRemoteFile("rename",path+"/missing-source","unused.txt").steps[0].args[1]).code!=0&&!std::filesystem::exists(root/L"unused.txt"),"missing rename source is not reported as success");
        auto injected="x'; touch CYVR_INJECTION_"+std::to_string(GetCurrentProcessId())+"; echo '";
        check(run(changeRemoteFile("mkdir",path,injected).steps[0].args[1]).code==0&&std::filesystem::is_directory(root/widen(injected))&&!std::filesystem::exists(base/("CYVR_INJECTION_"+std::to_string(GetCurrentProcessId()))),"shell metacharacters remain a literal filename");
        run(changeRemoteFile("delete",path+"/"+injected).steps[0].args[1]);
        auto push=pushRemoteFile(source.wstring(),path+"/uploaded.txt");check(run(push.steps[0].args[1]).code==0,"new upload destination accepted");
        check(run(push.steps[2].args[1]).code!=0&&!std::filesystem::exists(root/L"uploaded.txt"),"missing upload staging is not reported as success");
        auto staged=windows(push.steps[1].args[2]);std::filesystem::copy_file(source,staged);check(!std::filesystem::exists(root/L"uploaded.txt"),"upload stages before publishing target");
        check(run(push.steps[2].args[1]).code==0&&read(root/L"uploaded.txt")=="original payload\n"&&!std::filesystem::exists(staged),"upload finalizes by rename");
        check(run(push.steps[0].args[1]).code!=0&&read(root/L"uploaded.txt")=="original payload\n","upload no-overwrite preserves destination");
        auto replacement=pushRemoteFile(source.wstring(),path+"/occupied.txt",true);check(run(replacement.steps[0].args[1]).code==0,"explicit replacement accepted");
        auto replacementStage=windows(replacement.steps[1].args[2]);std::filesystem::copy_file(source,replacementStage);
        check(run(replacement.cleanupSteps[0].args[1]).code==0&&!std::filesystem::exists(replacementStage)&&read(root/L"occupied.txt")=="keep","cancel cleanup removes only staging, preserves prior destination");
        std::filesystem::copy_file(source,replacementStage);check(run(replacement.steps[2].args[1]).code==0&&read(root/L"occupied.txt")=="original payload\n"&&read(source)=="original payload\n","explicit replacement preserves original local source");
        check(rejects([&]{parseFileListing(run(fileListingScript(path+"/missing")).output);}),"missing-folder script produces useful failure");
        for(const auto& name:{L"renamed 'é.txt",L"occupied.txt",L"uploaded.txt"})std::filesystem::remove(root/name);
        check(std::filesystem::remove(root),"fixture cleaned without recursive deletion");
    }else std::cout<<"POSIX shell unavailable: command execution checks skipped\n";
    std::cout<<checks<<" file checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<"\n";return 1;}}
