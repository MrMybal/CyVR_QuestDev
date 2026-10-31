// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/transfers.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace quest;
static int checks{};
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>static bool rejects(F function){try{function();return false;}catch(const std::exception&){return true;}}
static void write(const std::filesystem::path& p,const std::string& text){std::ofstream f(p,std::ios::binary);f<<text;if(!f)throw std::runtime_error("Fixture write failed");}
static std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static FileTransfer download(const std::filesystem::path& path,const std::string& remote="/sdcard/readme.txt"){FileTransfer f;f.device="FAKE-QUEST";f.direction=TransferDirection::download;f.local=path.wstring();f.remote=remote;return f;}
int main(int argc,char** argv){try{
    auto base=std::filesystem::current_path().lexically_normal();auto root=base/(L"transfer-fixtures-"+std::to_wstring(GetCurrentProcessId()));check(!std::filesystem::exists(root),"fresh test directory");std::filesystem::create_directories(root/L"Dossier équipe");
    auto target=root/L"Dossier équipe/capture é.txt";auto first=download(target);TransferQueue queue;auto a=queue.add(first);auto second=download(root/L"two.txt");auto b=queue.add(second);
    check(queue.next()==nullptr&&!queue.active(),"preparation never dispatches before explicit start");
    check(rejects([&]{queue.start("OTHER-QUEST");})&&queue.next()==nullptr,"batch cannot run against another headset");
    queue.start("FAKE-QUEST");check(queue.next()&&queue.next()->id==a,"batch starts in preparation order");queue.started(a,101);check(queue.next()==nullptr&&queue.active(),"only one file runs at a time");
    check(queue.cancel(b)==0&&queue.count(TransferState::cancelled)==1,"waiting item cancellation never submits a job");
    check(queue.cancelAll()==101&&!queue.running(),"batch cancellation stops dispatch and identifies the owned active job");queue.finish(101,true,false,"cancelled");check(queue.count(TransferState::succeeded)==0&&queue.count(TransferState::cancelled)==2,"cancelled download is not reported successful after late acknowledgement");
    queue.retry();check(queue.count(TransferState::cancelled)==2,"retrying errors does not revive deliberately cancelled items");queue.retry(true);check(queue.count(TransferState::waiting)==2&&!queue.running()&&!queue.next(),"retry returns items to preparation without automatic execution");queue.start("FAKE-QUEST");queue.reject(a,"fixture missing");check(!queue.running()&&queue.count(TransferState::waiting)==1,"error pauses batch and retains unstarted items");queue.start("FAKE-QUEST");check(queue.next()->id==b,"resume skips failed items");queue.started(b,102);queue.finish(102,true,false,"");check(!queue.running()&&queue.count(TransferState::succeeded)==1,"last result completes the batch");queue.clearFinished();check(queue.items().empty(),"finished records can be cleared");
    queue.add(first);auto duplicate=first;duplicate.remote="/sdcard/other.txt";check(rejects([&]{queue.add(duplicate);}),"duplicate PC destinations are rejected");
    duplicate.local=target.wstring();for(auto& c:duplicate.local)if(c>=L'a'&&c<=L'z')c-=32;check(rejects([&]{queue.add(duplicate);}),"PC collision detection respects Windows case insensitivity");
    queue.add(second);queue.stopOnError=false;queue.start("FAKE-QUEST");queue.reject(queue.next()->id,"fixture error");check(queue.running()&&queue.next()!=nullptr,"optional continue-on-error processes the remaining batch");check(queue.pause()==0&&!queue.next()&&queue.count(TransferState::waiting)==1,"device change can suspend and preserve preparation");
    auto plan=prepareTransfer(first);write(plan.temporary,"partial");finishTransfer(plan,false);check(!std::filesystem::exists(plan.temporary)&&!std::filesystem::exists(target),"cancellation cleans partial download without creating destination");
    write(target,"original");check(rejects([&]{prepareTransfer(first);})&&read(target)=="original","default download refuses existing destination");
    first.overwrite=true;plan=prepareTransfer(first);write(plan.temporary,"replacement");finishTransfer(plan,false);check(read(target)=="original","cancelled replacement preserves original bytes");
    plan=prepareTransfer(first);write(plan.temporary,"replacement");finishTransfer(plan,true);check(read(target)=="replacement"&&!std::filesystem::exists(plan.temporary),"successful replacement is atomically finalized");
    auto race=download(root/L"race.txt");plan=prepareTransfer(race);write(plan.temporary,"download");write(race.local,"appeared after preparation");check(rejects([&]{finishTransfer(plan,true);})&&read(race.local)=="appeared after preparation"&&!std::filesystem::exists(plan.temporary),"destination race cannot silently overwrite a file");
    auto directory=download(root/L"Dossier équipe");directory.overwrite=true;check(rejects([&]{prepareTransfer(directory);}),"download never replaces a directory");
    FileTransfer upload;upload.device="FAKE-QUEST";upload.direction=TransferDirection::upload;upload.local=target.wstring();upload.remote="/sdcard/Download/a 'é.txt";auto push=prepareTransfer(upload);check(push.request.steps.size()==3&&!push.request.cleanupSteps.empty(),"uploads keep guarded commit and cancellation cleanup");upload.local=(root/L"missing.apk").wstring();check(rejects([&]{prepareTransfer(upload);}),"source is revalidated when prepared transfer starts");
    for(const auto* name:{"../escape.txt","C:\\escape","NUL.txt","CON","bad:name","ending.","newline\n.txt"})check(rejects([&]{localChild(root.wstring(),name);}),"Android names cannot escape or alias reserved Windows files");
    check(std::filesystem::path(localChild(root.wstring(),"équipe's file.txt")).parent_path()==root,"Unicode and spaces in PC destinations remain exact");auto listing=listLocalFiles(root.wstring());check(!listing.entries.empty()&&listing.entries.front().type=='d',"PC listing has exact directory metadata and directories first");check(rejects([&]{listLocalFiles((root/L"absent").wstring());}),"inaccessible PC folder gives an error");
    auto hidden=root/L"hidden-fixture.txt";write(hidden,"hidden fixture");check(SetFileAttributesW(hidden.c_str(),FILE_ATTRIBUTE_HIDDEN)!=FALSE,"Windows hidden fixture");auto hiddenListing=listLocalFiles(root.wstring());bool hiddenFound=false;for(const auto& entry:hiddenListing.entries)if(entry.name=="hidden-fixture.txt")hiddenFound=entry.hidden;check(hiddenFound,"PC listing preserves Windows hidden attributes");
    if(argc>1){
        Backend backend(widen(argv[1]));auto session=backend.select("FAKE-QUEST");TransferQueue real;auto item=download(root/L"actual transfer é.txt");auto id=real.add(item);real.start("FAKE-QUEST");auto actual=prepareTransfer(item);auto job=backend.submit(actual.request);real.started(id,job);bool finished=false;
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(std::chrono::steady_clock::now()<deadline&&!finished){for(const auto& e:backend.poll())if(e.kind==BackendEvent::JobDone&&e.job==job){check(e.session==session&&e.ok,"real backend completes guarded pull");finishTransfer(actual,e.ok);real.finish(job,e.ok,e.cancelled,e.text);finished=true;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        check(finished&&real.count(TransferState::succeeded)==1&&read(item.local)=="fixture Quest payload\n","queued download uses background ADB and exact bytes");
        auto cancelledItem=download(root/L"cancelled.txt");auto staged=prepareTransfer(cancelledItem);write(staged.temporary,"partial fixture");auto hanging=backend.submit({"owned transfer fixture",{{{"shell","job-hang"},{}}},30000,true});backend.cancel(hanging);finished=false;deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(std::chrono::steady_clock::now()<deadline&&!finished){for(const auto& e:backend.poll())if(e.kind==BackendEvent::JobDone&&e.job==hanging){check(e.cancelled&&!e.ok,"backend acknowledges cancellation of the owned job");finishTransfer(staged,false);finished=true;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        check(finished&&!std::filesystem::exists(staged.temporary)&&!std::filesystem::exists(cancelledItem.local),"cancelled background transfer leaves no partial destination");
    }
    auto resolved=std::filesystem::weakly_canonical(root);check(resolved.parent_path()==std::filesystem::weakly_canonical(base)&&resolved.filename().wstring().rfind(L"transfer-fixtures-",0)==0,"fixture cleanup stays inside its verified test directory");std::filesystem::remove_all(resolved);
    std::cout<<checks<<" transfer checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<"\n";return 1;}}
