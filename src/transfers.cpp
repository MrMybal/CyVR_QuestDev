// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/transfers.hpp"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace quest {
static bool sameDestination(const FileTransfer& a,const FileTransfer& b){
    if(a.direction!=b.direction)return false;
    if(a.direction==TransferDirection::upload)return a.device==b.device&&a.remote==b.remote;
    return CompareStringOrdinal(a.local.c_str(),-1,b.local.c_str(),-1,TRUE)==CSTR_EQUAL;
}
uint64_t TransferQueue::add(FileTransfer item){
    if(items_.size()>=2000)throw std::runtime_error("La liste est limitée à 2 000 fichiers. Effacez les transferts terminés.");
    if(item.device.empty()||item.local.empty())throw std::runtime_error("Casque et chemin local requis.");
    item.remote=remotePath(item.remote);if(item.remote=="/")throw std::runtime_error("Un chemin de fichier Android est requis.");
    std::filesystem::path local(item.local);if(!local.is_absolute()||local.filename().empty())throw std::runtime_error("Chemin local absolu de fichier requis.");
    item.local=local.lexically_normal().wstring();
    for(const auto& previous:items_)if(sameDestination(item,previous))throw std::runtime_error("Cette destination est déjà dans la liste. Retirez les transferts terminés pour la réutiliser.");
    item.id=nextId_++;item.job=0;item.state=TransferState::waiting;item.cancelRequested=false;item.error.clear();items_.push_back(std::move(item));return items_.back().id;
}
size_t TransferQueue::count(TransferState state)const{return size_t(std::count_if(items_.begin(),items_.end(),[&](const FileTransfer& i){return i.state==state;}));}
bool TransferQueue::active()const{return count(TransferState::running)!=0;}
void TransferQueue::start(const std::string& device){
    if(device.empty())throw std::runtime_error("Connectez le casque lié à la liste.");
    for(const auto& i:items_)if((i.state==TransferState::waiting||i.state==TransferState::running)&&i.device!=device)throw std::runtime_error("La liste contient des fichiers destinés à un autre casque. Reconnectez-le ou retirez ces lignes.");
    running_=active()||count(TransferState::waiting)!=0;
}
const FileTransfer* TransferQueue::next()const{if(!running_||active())return nullptr;for(const auto& i:items_)if(i.state==TransferState::waiting)return &i;return nullptr;}
void TransferQueue::started(uint64_t item,uint64_t job){
    auto* candidate=next();if(!candidate||candidate->id!=item||!job)throw std::runtime_error("Ordre de transfert invalide.");
    for(auto& i:items_)if(i.id==item){i.job=job;i.state=TransferState::running;return;}
}
void TransferQueue::reject(uint64_t item,const std::string& message){
    for(auto& i:items_)if(i.id==item&&i.state==TransferState::waiting){i.state=TransferState::failed;i.error=message;if(stopOnError)running_=false;break;}
    if(!active()&&!count(TransferState::waiting))running_=false;
}
bool TransferQueue::finish(uint64_t job,bool ok,bool cancelled,const std::string& message){
    for(auto& i:items_)if(i.state==TransferState::running&&i.job==job){
        i.state=cancelled||i.cancelRequested?TransferState::cancelled:ok?TransferState::succeeded:TransferState::failed;
        i.error=i.state==TransferState::succeeded?"":message;
        if(i.state==TransferState::failed&&stopOnError)running_=false;
        if(!count(TransferState::waiting))running_=false;return true;
    }return false;
}
uint64_t TransferQueue::cancel(uint64_t item){for(auto& i:items_)if(i.id==item){if(i.state==TransferState::waiting)i.state=TransferState::cancelled;else if(i.state==TransferState::running){i.cancelRequested=true;return i.job;}}if(!active()&&!count(TransferState::waiting))running_=false;return 0;}
uint64_t TransferQueue::pause(){running_=false;for(auto& i:items_)if(i.state==TransferState::running){i.cancelRequested=true;return i.job;}return 0;}
uint64_t TransferQueue::cancelAll(){auto job=pause();for(auto& i:items_)if(i.state==TransferState::waiting)i.state=TransferState::cancelled;return job;}
void TransferQueue::retry(bool includeCancelled){if(active())throw std::runtime_error("Attendez la fin du transfert actif.");for(auto& i:items_)if(i.state==TransferState::failed||(includeCancelled&&i.state==TransferState::cancelled)){i.state=TransferState::waiting;i.error.clear();i.job=0;i.cancelRequested=false;}}
void TransferQueue::clearFinished(){items_.erase(std::remove_if(items_.begin(),items_.end(),[](const FileTransfer& i){return i.state!=TransferState::waiting&&i.state!=TransferState::running;}),items_.end());}

std::wstring localChild(const std::wstring& folder,const std::string& name){
    if(name.empty()||name=="."||name==".."||name.size()>255||name.back()=='.'||name.back()==' '||name.find_first_of("\\/:*?\"<>|")!=std::string::npos||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;}))throw std::runtime_error("Nom de fichier incompatible avec Windows : "+name);
    auto stem=lower(name.substr(0,name.find('.')));
    if(stem=="con"||stem=="prn"||stem=="aux"||stem=="nul"||(stem.size()==4&&(stem.rfind("com",0)==0||stem.rfind("lpt",0)==0)&&stem[3]>='0'&&stem[3]<='9'))throw std::runtime_error("Nom de fichier réservé par Windows.");
    return (std::filesystem::path(folder)/widen(name)).wstring();
}
RemoteListing listLocalFiles(const std::wstring& folder){
    WIN32_FIND_DATAW data{};auto pattern=(std::filesystem::path(folder)/L"*").wstring();auto handle=FindFirstFileExW(pattern.c_str(),FindExInfoBasic,&data,FindExSearchNameMatch,nullptr,FIND_FIRST_EX_LARGE_FETCH);
    if(handle==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_NOT_FOUND)return {};throw std::runtime_error("Dossier PC absent ou inaccessible (Windows "+std::to_string(GetLastError())+").");}
    RemoteListing result;
    do{
        if(wcscmp(data.cFileName,L".")==0||wcscmp(data.cFileName,L"..")==0)continue;
        if(result.entries.size()>=5000){result.truncated=true;break;}
        RemoteFile entry;entry.name=narrow(data.cFileName);entry.hidden=(data.dwFileAttributes&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))!=0;entry.type=(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)?'l':(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?'d':'f';
        if(entry.type=='f')entry.size=(uint64_t(data.nFileSizeHigh)<<32)|data.nFileSizeLow;
        auto stamp=(uint64_t(data.ftLastWriteTime.dwHighDateTime)<<32)|data.ftLastWriteTime.dwLowDateTime;if(stamp>=116444736000000000ULL)entry.modified=int64_t((stamp-116444736000000000ULL)/10000000);
        result.entries.push_back(std::move(entry));
    }while(FindNextFileW(handle,&data));
    auto error=GetLastError();FindClose(handle);if(!result.truncated&&error!=ERROR_NO_MORE_FILES)throw std::runtime_error("Lecture du dossier PC interrompue.");
    std::sort(result.entries.begin(),result.entries.end(),[](const RemoteFile& a,const RemoteFile& b){if((a.type=='d')!=(b.type=='d'))return a.type=='d';return lower(a.name)<lower(b.name);});return result;
}
static void checkDestination(const std::wstring& path,bool overwrite){
    auto attributes=GetFileAttributesW(path.c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES){
        if(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("La destination PC est un dossier ou un lien.");
        if(!overwrite)throw std::runtime_error("Le fichier PC existe déjà. Le remplacement est désactivé.");
    }else if(GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND)throw std::runtime_error("Impossible de vérifier la destination PC.");
}
TransferPlan prepareTransfer(const FileTransfer& item){
    TransferPlan plan;
    if(item.direction==TransferDirection::upload){
        auto attr=GetFileAttributesW(item.local.c_str());if(attr==INVALID_FILE_ATTRIBUTES||(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))throw std::runtime_error("Le fichier PC est absent, inaccessible ou un lien.");
        plan.request=pushRemoteFile(item.local,item.remote,item.overwrite);
    }else{
        checkDestination(item.local,item.overwrite);
        auto parent=std::filesystem::path(item.local).parent_path();std::error_code ec;if(!std::filesystem::is_directory(parent,ec))throw std::runtime_error("Le dossier de destination PC n’existe plus.");
        static std::atomic<uint64_t> sequence{};
        plan.temporary=item.local+L".questdev-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count())+L"-"+std::to_wstring(++sequence)+L".part";
        plan.destination=item.local;plan.overwrite=item.overwrite;plan.request=pullRemoteFile(item.remote,plan.temporary);
    }
    plan.request.timeoutMs=30*60*1000;return plan;
}
void finishTransfer(const TransferPlan& plan,bool success){
    if(plan.temporary.empty())return;
    try{
        if(success){checkDestination(plan.destination,plan.overwrite);if(!MoveFileExW(plan.temporary.c_str(),plan.destination.c_str(),MOVEFILE_WRITE_THROUGH|(plan.overwrite?MOVEFILE_REPLACE_EXISTING:0)))throw std::runtime_error("Finalisation du fichier PC impossible (Windows "+std::to_string(GetLastError())+").");}
    }catch(...){std::error_code ec;std::filesystem::remove(plan.temporary,ec);throw;}
    std::error_code ec;std::filesystem::remove(plan.temporary,ec);
    if(ec)throw std::runtime_error("Le fichier partiel PC reste verrouillé : "+ec.message());
}
}
