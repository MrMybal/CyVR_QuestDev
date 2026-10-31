// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/file_workspace.hpp"
#include <cyui/CyUI.h>
#include <imgui.h>
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <set>

namespace quest {
namespace {
std::string label(const std::string& name){std::string out;for(unsigned char c:name){if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c<32||c==127)out+='?';else out+=char(c);}return out;}
std::string sizeText(const std::optional<uint64_t>& bytes){if(!bytes)return "—";double n=double(*bytes);const char* units[]={"o","Ko","Mo","Go","To"};int i=0;while(n>=1024&&i<4){n/=1024;++i;}char text[64];std::snprintf(text,sizeof(text),i?"%.1f %s":"%.0f %s",n,units[i]);return text;}
std::wstring chooseFolder(){IFileOpenDialog* d{};std::wstring out;if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&d)))){d->SetOptions(FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);if(SUCCEEDED(d->Show(GetActiveWindow()))){IShellItem* i{};if(SUCCEEDED(d->GetResult(&i))){PWSTR p{};if(SUCCEEDED(i->GetDisplayName(SIGDN_FILESYSPATH,&p))){out=p;CoTaskMemFree(p);}i->Release();}}d->Release();}return out;}
std::vector<std::wstring> chooseFiles(){IFileOpenDialog* d{};std::vector<std::wstring> out;if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&d)))){d->SetOptions(FOS_FILEMUSTEXIST|FOS_PATHMUSTEXIST|FOS_FORCEFILESYSTEM|FOS_ALLOWMULTISELECT);if(SUCCEEDED(d->Show(GetActiveWindow()))){IShellItemArray* a{};if(SUCCEEDED(d->GetResults(&a))){DWORD n{};a->GetCount(&n);for(DWORD j=0;j<std::min(n,DWORD(2000));++j){IShellItem* i{};if(SUCCEEDED(a->GetItemAt(j,&i))){PWSTR p{};if(SUCCEEDED(i->GetDisplayName(SIGDN_FILESYSPATH,&p))){out.emplace_back(p);CoTaskMemFree(p);}i->Release();}}a->Release();}}d->Release();}return out;}
std::wstring saveFile(const std::string& name){wchar_t text[32768]{};wcsncpy_s(text,widen(name).c_str(),_TRUNCATE);OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=GetActiveWindow();ofn.lpstrFile=text;ofn.nMaxFile=32768;ofn.lpstrFilter=L"Tous les fichiers\0*.*\0\0";ofn.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;return GetSaveFileNameW(&ofn)?text:L"";}
RemoteListing readListing(const std::filesystem::path& p){std::error_code ec;if(std::filesystem::file_size(p,ec)>4*1024*1024||ec)throw std::runtime_error("Liste de fichiers absente ou trop grande.");std::ifstream f(p,std::ios::binary);return parseFileListing(std::string(std::istreambuf_iterator<char>(f),{}));}
const char* stateName(TransferState s){switch(s){case TransferState::waiting:return "En attente";case TransferState::running:return "Transfert";case TransferState::succeeded:return "Terminé";case TransferState::failed:return "Échec";default:return "Annulé";}}
}
struct FileWorkspace::Impl {
    struct Pane {std::string folder,error;std::array<char,4097> address{};std::array<char,256> filter{};RemoteListing listing;std::set<std::string> selected;int anchor{-1};bool loaded{};};
    struct Active {TransferPlan plan;uint64_t item{};float percent{};};
    std::filesystem::path data;
    bool demo{},split{},prepare{},hidden{true},overwrite{},showQueue{},online{},loadedRemote{},dialog{};
    Submit submit;Cancel cancel;std::function<void()> save;
    std::string device,message,dialogTarget;
    uint64_t session{},listingJob{},sequence{};
    int action{};
    std::array<char,256> newName{},packageFilter{};
    Pane local,remote;
    TransferQueue queue;
    std::map<uint64_t,std::filesystem::path> listings;
    std::map<uint64_t,Active> active;
    std::set<uint64_t> changes;
    Impl(std::filesystem::path path,bool simulated,bool side,bool queued,Submit s,Cancel c,std::function<void()> persist):data(std::move(path)),demo(simulated),split(side),prepare(queued),showQueue(queued),submit(std::move(s)),cancel(std::move(c)),save(std::move(persist)){
        remote.folder="/sdcard";
        PWSTR docs{};if(!demo&&SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&docs))){local.folder=narrow(docs);CoTaskMemFree(docs);}else local.folder="C:\\QuestDev\\Workspace";
        copyAddress(local);copyAddress(remote);
    }
    ~Impl(){for(const auto& [id,p]:listings){std::error_code ec;std::filesystem::remove(p,ec);}for(const auto& [id,a]:active)try{finishTransfer(a.plan,false);}catch(...){} }
    static void copyAddress(Pane& p){strncpy_s(p.address.data(),p.address.size(),p.folder.c_str(),_TRUNCATE);}
    void browse(bool pc,const std::string& folder){
        auto& p=pc?local:remote;p.error.clear();p.selected.clear();p.anchor=-1;
        try{
            p.folder=pc?narrow(std::filesystem::absolute(widen(folder)).lexically_normal().wstring()):remotePath(folder);copyAddress(p);p.listing={};p.loaded=true;
            if(demo){
                if(pc)p.listing.entries={{"Builds","",'d',{},1793289600},{"Documents","",'d',{},1793289600},{"quest-demo.apk","",'f',137573171,1793289600},{"session-équipe.json","",'f',2048,1793289600},{"README.txt","",'f',15480,1793289600}};
                else if(p.folder=="/sdcard")p.listing.entries={{"Download","",'d',{},1793289600},{"Documents","",'d',{},1793289600},{"Movies","",'d',{},1793289600},{"Android","",'d',{},1793289600},{"quest-session.txt","",'f',15480,1793289600}};
                else p.listing.entries={{"captures","",'d',{},1793289600},{"capture de test.png","",'f',524288,1793289600},{"scene-vr.apk","",'f',137573171,1793289600},{"session-équipe.json","",'f',2048,1793289600},{".settings","",'f',512,1793289600}};
                return;
            }
            if(pc){p.listing=listLocalFiles(widen(p.folder));return;}
            if(!online){p.error="Connectez un Quest autorisé.";return;}
            if(listingJob)cancel(listingJob);
            auto folderPath=data/L"cache";std::filesystem::create_directories(folderPath);auto output=folderPath/(L"browser-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++sequence)+L".bin");auto job=listRemoteFiles(p.folder,output.wstring());listingJob=submit(std::move(job),output.wstring());listings[listingJob]=output;
        }catch(const std::exception& e){p.error=e.what();}
    }
    std::vector<const RemoteFile*> selection(const Pane& p)const{std::vector<const RemoteFile*> out;for(const auto& f:p.listing.entries)if(f.type=='f'&&p.selected.count(f.name))out.push_back(&f);return out;}
    void add(std::vector<FileTransfer> files){
        if(demo||device.empty()||(!online&&!prepare))return;size_t count=0;std::string error;bool wasRunning=queue.running();auto previousPending=queue.count(TransferState::waiting);
        for(auto& f:files)try{queue.add(std::move(f));++count;}catch(const std::exception& e){error=e.what();}
        if(!count&&error.empty())return;
        showQueue=true;message=std::to_string(count)+" fichier(s) ajouté(s)";
        if(!error.empty())message+=" · "+error;
        else if(!prepare){
            if(wasRunning||!previousPending){try{queue.start(device);}catch(const std::exception& e){message=e.what();}}
            else message+=" · relancez le lot pour reprendre les lignes en attente";
        }
        cyui::notify(message.c_str(),error.empty()?cyui::Tone::info:cyui::Tone::warning);
    }
    void upload(const std::vector<std::wstring>& paths){
        try{
        std::vector<FileTransfer> files;for(const auto& path:paths){FileTransfer f;f.direction=TransferDirection::upload;f.device=device;f.local=path;f.remote=remoteChild(remote.folder,narrow(std::filesystem::path(path).filename().wstring()));f.overwrite=overwrite;std::error_code ec;auto bytes=std::filesystem::file_size(path,ec);if(!ec)f.size=bytes;files.push_back(std::move(f));}add(std::move(files));
        }catch(const std::exception& e){message=e.what();cyui::notify(e.what(),cyui::Tone::error);}
    }
    void downloadSelection(){
        try{
        auto selected=selection(remote);if(selected.empty())return;std::vector<FileTransfer> files;
        std::wstring folder;bool singleSave=!split&&selected.size()==1;
        if(!split&&!singleSave){folder=chooseFolder();if(folder.empty())return;}
        for(const auto* file:selected){
            FileTransfer f;f.direction=TransferDirection::download;f.device=device;f.remote=remoteChild(remote.folder,file->name);f.size=file->size;f.overwrite=overwrite;
            if(singleSave){f.local=saveFile(file->name);if(f.local.empty())return;f.overwrite=true;}
            else f.local=localChild(split?widen(local.folder):folder,file->name);
            files.push_back(std::move(f));
        }add(std::move(files));
        }catch(const std::exception& e){message=e.what();cyui::notify(e.what(),cyui::Tone::error);}
    }
    void cancelJob(uint64_t job){if(job&&!demo)cancel(job);}
    void tick(const std::string& serial,uint64_t sid,bool connected,bool visible){
        if(device!=serial||session!=sid||(!connected&&online)){
            cancelJob(queue.pause());if(listingJob)cancelJob(listingJob);
            if(device!=serial||session!=sid){remote.selected.clear();remote.listing={};loadedRemote=false;listingJob=0;}
            loadedRemote=false;
            if(!demo&&!queue.items().empty())message="Lot suspendu : reconnectez son casque, puis relancez-le.";
        }
        device=serial;session=sid;online=connected;
        if(visible&&!loadedRemote&&(online||demo)){loadedRemote=true;browse(false,remote.folder);}
        if(!demo&&online)if(const auto* item=queue.next()){
            auto id=item->id;
            try{if(item->device!=device)throw std::runtime_error("Le casque de la liste n’est pas sélectionné.");auto plan=prepareTransfer(*item);auto job=submit(plan.request,plan.temporary);queue.started(id,job);active[job]={std::move(plan),id};message="Transfert en cours…";}
            catch(const std::exception& e){queue.reject(id,e.what());message=e.what();}
        }
    }
    std::optional<FileCompletion> event(const BackendEvent& e){
        auto a=active.find(e.job);
        if(e.kind==BackendEvent::JobOutput&&a!=active.end()){
            auto percent=e.text.rfind('%');if(percent!=std::string::npos){auto at=percent;while(at>0&&e.text[at-1]>='0'&&e.text[at-1]<='9')--at;if(at<percent)try{a->second.percent=float(std::clamp(std::stoi(e.text.substr(at,percent-at)),0,100));}catch(...){} }
        }
        if(e.kind!=BackendEvent::JobDone)return {};
        if(auto it=listings.find(e.job);it!=listings.end()){
            bool ok=e.ok&&e.session==session;
            if(e.job==listingJob){listingJob=0;try{if(ok)remote.listing=readListing(it->second);else if(!e.cancelled)remote.error=e.text;}catch(const std::exception& error){ok=false;remote.error=error.what();}}
            std::error_code ec;std::filesystem::remove(it->second,ec);listings.erase(it);return FileCompletion{ok,""};
        }
        if(a!=active.end()){
            bool cancelled=e.cancelled||e.session!=session;bool ok=e.ok&&!cancelled;
            for(const auto& item:queue.items())if(item.id==a->second.item)cancelled=cancelled||item.cancelRequested;
            ok=ok&&!cancelled;std::string error=cancelled?"Transfert annulé.":e.text;
            try{finishTransfer(a->second.plan,ok);}catch(const std::exception& ex){ok=false;error=ex.what();}
            queue.finish(e.job,ok,cancelled,error);active.erase(a);
            message=ok?"Transfert terminé.":error;
            if(!queue.running()&&!queue.active())cyui::notify(queue.count(TransferState::waiting)?"Lot suspendu. Consultez les erreurs.":"Lot terminé.",queue.count(TransferState::failed)?cyui::Tone::warning:cyui::Tone::info);
            if(e.session==session&&ok){local.loaded=false;if(!queue.running())browse(false,remote.folder);}
            return FileCompletion{ok,error};
        }
        if(changes.erase(e.job)){if(e.session==session){message=e.text;if(e.ok)browse(false,remote.folder);cyui::notify(e.ok?"Opération terminée.":e.text.c_str(),e.ok?cyui::Tone::success:cyui::Tone::error);}return FileCompletion{e.ok,e.text};}
        return {};
    }
    void table(Pane& p,bool pc){
        std::vector<int> visible;auto query=lower(p.filter.data());for(size_t j=0;j<p.listing.entries.size();++j){const auto& f=p.listing.entries[j];if((hidden||(!f.hidden&&f.name[0]!='.'))&&lower(f.name).find(query)!=std::string::npos)visible.push_back(int(j));}
        bool dates=ImGui::GetContentRegionAvail().x>cyui::dp(590);std::optional<std::string> open;
        auto flags=ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY|ImGuiTableFlags_BordersInnerV;
        if(ImGui::BeginTable("##entries",dates?4:3,flags,ImVec2(0,-cyui::dp(28)))){
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableSetupColumn("##selection",ImGuiTableColumnFlags_WidthFixed,cyui::dp(26));ImGui::TableSetupColumn("Nom",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Taille",ImGuiTableColumnFlags_WidthFixed,cyui::dp(76));if(dates)ImGui::TableSetupColumn("Modification",ImGuiTableColumnFlags_WidthFixed,cyui::dp(132));
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);ImGui::TableNextColumn();bool all=std::any_of(visible.begin(),visible.end(),[&](int j){return p.listing.entries[size_t(j)].type=='f';})&&std::all_of(visible.begin(),visible.end(),[&](int j){const auto& f=p.listing.entries[size_t(j)];return f.type!='f'||p.selected.count(f.name);});
            if(ImGui::Checkbox("##all",&all)){p.selected.clear();if(all)for(int j:visible)if(p.listing.entries[size_t(j)].type=='f')p.selected.insert(p.listing.entries[size_t(j)].name);}cyui::tooltip("Sélectionner tous les fichiers visibles (Ctrl+A dans ce volet)");ImGui::TableNextColumn();ImGui::TableHeader("Nom");ImGui::TableNextColumn();ImGui::TableHeader("Taille");if(dates){ImGui::TableNextColumn();ImGui::TableHeader("Modification");}
            ImGuiListClipper clip;clip.Begin(int(visible.size()));while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row){
                const auto& f=p.listing.entries[size_t(visible[size_t(row)])];ImGui::PushID(visible[size_t(row)]);ImGui::TableNextRow();bool selected=p.selected.count(f.name)!=0;if(selected)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,cyui::palette().selection);
                ImGui::TableNextColumn();if(ImGui::Checkbox("##select",&selected)){if(selected)p.selected.insert(f.name);else p.selected.erase(f.name);p.anchor=row;}
                ImGui::TableNextColumn();auto pos=ImGui::GetCursorScreenPos();auto width=ImGui::GetContentRegionAvail().x;
                if(ImGui::Selectable("##name",selected,ImGuiSelectableFlags_AllowDoubleClick,ImVec2(0,ImGui::GetTextLineHeight()))){
                    if(ImGui::GetIO().KeyShift&&p.anchor>=0){for(int k=std::min(row,p.anchor);k<=std::max(row,p.anchor)&&k<int(visible.size());++k)p.selected.insert(p.listing.entries[size_t(visible[size_t(k)])].name);}
                    else if(ImGui::GetIO().KeyCtrl){if(selected)p.selected.erase(f.name);else p.selected.insert(f.name);p.anchor=row;}
                    else{p.selected={f.name};p.anchor=row;}
                    if(f.type=='d'&&ImGui::IsMouseDoubleClicked(0))open=pc?narrow((std::filesystem::path(widen(p.folder))/widen(f.name)).wstring()):remotePath(p.folder+"/"+f.name);
                }
                if(ImGui::BeginPopupContextItem("##entryMenu")){
                    if(ImGui::MenuItem("Copier le chemin")){auto path=pc?narrow((std::filesystem::path(widen(p.folder))/widen(f.name)).wstring()):remotePath(p.folder+"/"+f.name);ImGui::SetClipboardText(path.c_str());}
                    if(f.type=='l')cyui::textDim("Lien non suivi : %s",label(f.target).c_str());
                    ImGui::EndPopup();
                }
                auto text=std::string(cyui::iconsAvailable()?(f.type=='d'?cyui::icon::folder:f.type=='l'?cyui::icon::link:cyui::icon::document):"")+"  "+label(f.name);
                cyui::clippedText(ImGui::GetWindowDrawList(),pos,width,cyui::palette().text,text.c_str());cyui::tooltip(label(f.name).c_str());
                ImGui::TableNextColumn();cyui::textDim("%s",f.type=='d'?"—":sizeText(f.size).c_str());
                if(dates){ImGui::TableNextColumn();std::string date="—";if(f.modified){time_t seconds=time_t(*f.modified);tm t{};if(localtime_s(&t,&seconds)==0){char buf[40];strftime(buf,sizeof(buf),"%d/%m/%Y %H:%M",&t);date=buf;}}cyui::textFaint("%s",date.c_str());}ImGui::PopID();
            }ImGui::EndTable();
        }
        cyui::textFaint("%zu / %zu entrées · %zu sélection(s)",visible.size(),p.listing.entries.size(),p.selected.size());
        if(open)browse(pc,*open);
        if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)&&!ImGui::GetIO().WantTextInput){if(ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_A)){p.selected.clear();for(int j:visible)if(p.listing.entries[size_t(j)].type=='f')p.selected.insert(p.listing.entries[size_t(j)].name);}if(ImGui::IsKeyPressed(ImGuiKey_F5))browse(pc,p.folder);if(ImGui::GetIO().KeyAlt&&ImGui::IsKeyPressed(ImGuiKey_UpArrow))browse(pc,pc?narrow(std::filesystem::path(widen(p.folder)).parent_path().wstring()):remoteParent(p.folder));}
    }
    void pane(bool pc){
        auto& p=pc?local:remote;ImGui::PushID(pc?"pc":"quest");cyui::heading(pc?"PC":"QUEST",3);
        if(pc&&!p.loaded)browse(true,p.folder);
        if(cyui::iconButton(cyui::icon::back,"Dossier parent (Alt+Haut)"))browse(pc,pc?narrow(std::filesystem::path(widen(p.folder)).parent_path().wstring()):remoteParent(p.folder));ImGui::SameLine();
        if(cyui::iconButton(cyui::icon::refresh,"Actualiser ce volet (F5)"))browse(pc,p.folder);ImGui::SameLine();
        ImGui::SetNextItemWidth(-cyui::dp(40));bool go=ImGui::InputText("##address",p.address.data(),p.address.size(),ImGuiInputTextFlags_EnterReturnsTrue);ImGui::SameLine();
        if(cyui::iconButton(pc?cyui::icon::folder:cyui::icon::forward,pc?"Choisir un dossier PC (clic)":"Ouvrir le dossier Quest (Entrée)")){if(pc){auto path=chooseFolder();if(!path.empty())browse(true,narrow(path));}else go=true;}if(go)browse(pc,p.address.data());
        if(cyui::searchField("##filter",p.filter.data(),int(p.filter.size()),"Filtrer les noms",-1))p.anchor=-1;
        if(!p.error.empty())cyui::callout(cyui::Tone::error,p.error.substr(0,500).c_str());
        if(p.listing.truncated)cyui::textFaint("Liste limitée à 5 000 entrées.");
        if(!pc&&listingJob){cyui::progressBar(-1,ImVec2(-cyui::dp(72),cyui::dp(4)));ImGui::SameLine();if(cyui::ghostButton("Annuler"))cancelJob(listingJob);}
        auto selected=selection(p);
        {cyui::ScopedDisabled disabled(demo||(!online&&(!prepare||device.empty())));
            if(pc){cyui::ScopedDisabled noSelection(selected.empty());if(cyui::iconTextButton(cyui::icon::upload,prepare?"Ajouter vers Quest":"Envoyer vers Quest",prepare?cyui::Variant::secondary:cyui::Variant::primary)){std::vector<std::wstring> paths;for(const auto* f:selected)paths.push_back((std::filesystem::path(widen(p.folder))/widen(f->name)).wstring());upload(paths);}}
            else{cyui::ScopedDisabled noSelection(selected.empty());if(cyui::iconTextButton(cyui::icon::download,prepare?"Ajouter vers PC":split?"Récupérer vers PC":"Récupérer…",cyui::Variant::secondary))downloadSelection();}
            if(!pc){cyui::ScopedDisabled offline(!online);ImGui::SameLine();if(cyui::iconButton(cyui::icon::add,"Créer un dossier Quest (clic)")){action=1;dialogTarget=p.folder;newName.fill(0);dialog=true;}
                if(p.selected.size()==1){const auto& name=*p.selected.begin();ImGui::SameLine();if(cyui::iconButton(cyui::icon::rename,"Renommer la sélection Quest (clic)")){action=2;dialogTarget=remotePath(p.folder+"/"+name);strncpy_s(newName.data(),newName.size(),name.c_str(),_TRUNCATE);dialog=true;}ImGui::SameLine();if(cyui::iconButton(cyui::icon::trash,"Supprimer le fichier ou dossier vide Quest (clic)")){action=3;dialogTarget=remotePath(p.folder+"/"+name);dialog=true;}}
            }
        }
        if(!pc&&!online&&!demo)cyui::emptyState("Quest déconnecté","Le volet PC et la liste préparée restent disponibles.",cyui::icon::connect);
        else if(p.listing.entries.empty()&&!listingJob&&p.error.empty())cyui::emptyState("Dossier vide","Ouvrez un autre dossier ou ajoutez un fichier.",cyui::icon::folder);
        else table(p,pc);
        ImGui::PopID();
    }
    void queuePanel(){
        const auto& items=queue.items();auto pending=queue.count(TransferState::waiting),done=queue.count(TransferState::succeeded),failed=queue.count(TransferState::failed),cancelled=queue.count(TransferState::cancelled);
        cyui::heading(("Liste de transferts · "+std::to_string(items.size())+" fichier(s)").c_str(),3);
        {cyui::ScopedDisabled disabled(demo||!online||queue.running()||!pending);if(cyui::button("Lancer le lot",prepare?cyui::Variant::primary:cyui::Variant::secondary))try{queue.start(device);message="Lot lancé.";}catch(const std::exception& e){message=e.what();}}
        ImGui::SameLine();{cyui::ScopedDisabled disabled(demo||(!queue.active()&&!pending));if(cyui::button("Annuler le lot")){cancelJob(queue.cancelAll());message="Lot annulé. Les fichiers déjà transférés sont conservés.";}}
        ImGui::SameLine();{cyui::ScopedDisabled disabled(queue.active());if(cyui::ghostButton("Remettre en attente…"))ImGui::OpenPopup("##retry");if(ImGui::BeginPopup("##retry")){if(ImGui::MenuItem("Échecs uniquement")){queue.retry(false);message="Échecs remis en attente. Relancez le lot.";}if(ImGui::MenuItem("Échecs et annulations")){queue.retry(true);message="Échecs et annulations remis en attente. Relancez le lot.";}ImGui::EndPopup();}}
        ImGui::SameLine();if(cyui::ghostButton("Retirer terminés"))queue.clearFinished();
        ImGui::SameLine();cyui::toggleSwitch("Arrêt sur erreur",&queue.stopOnError);
        float fraction=items.empty()?0:float(done+failed+cancelled)/float(items.size());if(!active.empty()&&!items.empty())fraction+=active.begin()->second.percent/100.f/float(items.size());
        auto progress=std::to_string(done)+" terminé(s) · "+std::to_string(pending)+" en attente · "+std::to_string(failed)+" échec(s) · "+std::to_string(cancelled)+" annulé(s)";
        cyui::progressBar(fraction,ImVec2(-1,cyui::dp(16)),progress.c_str(),failed?cyui::Tone::warning:cyui::Tone::info);
        if(!message.empty())cyui::textDim("%s",message.substr(0,300).c_str());
        if(items.empty()){cyui::textFaint("Sélectionnez plusieurs fichiers, ajoutez-les à la liste, puis lancez le lot. La liste reste dans cette session.");return;}
        if(ImGui::BeginTable("##transfers",5,ImGuiTableFlags_ScrollY|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable,ImVec2(0,0))){
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableSetupColumn("Sens",ImGuiTableColumnFlags_WidthFixed,cyui::dp(82));ImGui::TableSetupColumn("Fichier / destination",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Taille",ImGuiTableColumnFlags_WidthFixed,cyui::dp(80));ImGui::TableSetupColumn("État",ImGuiTableColumnFlags_WidthFixed,cyui::dp(115));ImGui::TableSetupColumn("##actions",ImGuiTableColumnFlags_WidthFixed,cyui::dp(42));ImGui::TableHeadersRow();
            ImGuiListClipper clip;clip.Begin(int(items.size()));while(clip.Step())for(int index=clip.DisplayStart;index<clip.DisplayEnd;++index){const auto& item=items[size_t(index)];ImGui::PushID(int(item.id));ImGui::TableNextRow();ImGui::TableNextColumn();cyui::textDim(item.direction==TransferDirection::upload?"PC → Quest":"Quest → PC");ImGui::TableNextColumn();auto filename=item.direction==TransferDirection::upload?narrow(std::filesystem::path(item.local).filename().wstring()):item.remote.substr(item.remote.rfind('/')+1);ImGui::TextUnformatted(label(filename).c_str());
                auto detail=label(narrow(item.local))+"\n"+label(item.remote)+"\n"+(item.overwrite?"Remplacement autorisé":"Sans remplacement")+(item.error.empty()?"":"\n"+item.error.substr(0,1000));cyui::tooltip(detail.c_str());
                cyui::textFaint("%s%s",item.overwrite?"REMPLACE · ":"",label(item.direction==TransferDirection::upload?item.remote:narrow(item.local)).c_str());
                ImGui::TableNextColumn();cyui::textDim("%s",sizeText(item.size).c_str());ImGui::TableNextColumn();cyui::badge(item.cancelRequested&&item.state==TransferState::running?"Annulation…":stateName(item.state),item.state==TransferState::succeeded?cyui::Tone::success:item.state==TransferState::failed?cyui::Tone::error:cyui::Tone::neutral);ImGui::TableNextColumn();if(item.state==TransferState::waiting||item.state==TransferState::running)if(cyui::iconButton(cyui::icon::close,"Annuler ce transfert (clic)"))cancelJob(queue.cancel(item.id));ImGui::PopID();
            }ImGui::EndTable();
        }
    }
    void draw(){
        cyui::panelHeader("Fichiers","PC et Quest · sélection multiple · transferts immédiats ou liste préparée.");
        bool changed=cyui::toggleSwitch("Côte à côte",&split);ImGui::SameLine();bool modeChanged=cyui::toggleSwitch("Préparer une liste",&prepare);
        if(modeChanged){if(prepare)showQueue=true;else if(!queue.running()&&queue.count(TransferState::waiting)){prepare=true;cyui::notify("Lancez ou annulez la liste préparée avant de passer aux transferts immédiats.",cyui::Tone::info);}changed=true;}if(changed)save();
        ImGui::SameLine();cyui::toggleSwitch("Fichiers cachés",&hidden);ImGui::SameLine();cyui::toggleSwitch("Remplacer",&overwrite);cyui::tooltip("Autoriser le remplacement pour les nouveaux transferts ajoutés. Les lignes déjà préparées conservent leur choix.");
        {cyui::ScopedDisabled disabled(demo||(!online&&(!prepare||device.empty())));if(cyui::iconTextButton(cyui::icon::upload,prepare?"Ajouter des fichiers PC…":"Envoyer des fichiers PC…",split||prepare?cyui::Variant::secondary:cyui::Variant::primary))upload(chooseFiles());}
        ImGui::SameLine();if(cyui::ghostButton(showQueue?"Masquer la liste":"Afficher la liste"))showQueue=!showQueue;
        if(overwrite)cyui::textColoured(cyui::palette().warning,"Remplacement activé pour les nouveaux transferts.");
        float available=ImGui::GetContentRegionAvail().y;bool queueVisible=showQueue||queue.active();float queueHeight=showQueue?std::min(cyui::dp(255),std::max(cyui::dp(165),available*.35f)):queue.active()?cyui::dp(68):0;
        ImGui::BeginChild("##filePanes",ImVec2(0,queueVisible?-queueHeight:0));
        if(split&&ImGui::BeginTable("##fileSplit",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_SizingStretchSame)){
            ImGui::TableNextColumn();ImGui::BeginChild("##pcPane");pane(true);ImGui::EndChild();ImGui::TableNextColumn();ImGui::BeginChild("##questPane");pane(false);ImGui::EndChild();ImGui::EndTable();
        }else if(!split){ImGui::BeginChild("##questPane");pane(false);ImGui::EndChild();}ImGui::EndChild();
        if(queueVisible){ImGui::BeginChild("##queue");if(showQueue)queuePanel();else{cyui::textDim("Transfert en cours · %zu fichier(s) en attente",queue.count(TransferState::waiting));if(cyui::button("Voir la liste"))showQueue=true;ImGui::SameLine();if(cyui::ghostButton("Annuler le lot"))cancelJob(queue.cancelAll());}ImGui::EndChild();}
        if(cyui::beginModal("Fichier du Quest",&dialog,cyui::dp(580))){
            const char* names[]={"","Créer un dossier","Renommer l’entrée","Supprimer l’entrée"};cyui::heading(names[std::clamp(action,0,3)],2);ImGui::TextWrapped("%s",label(dialogTarget).c_str());
            if(action!=3){ImGui::SetNextItemWidth(-1);ImGui::InputText("##newName",newName.data(),newName.size());}else cyui::callout(cyui::Tone::warning,"La suppression est définitive. Un dossier doit être vide ; les liens ne sont pas suivis.");
            if(cyui::modalButtons(action==3?"Supprimer":"Exécuter","Annuler",action==3)==1)try{auto job=changeRemoteFile(action==1?"mkdir":action==2?"rename":"delete",dialogTarget,newName.data());changes.insert(submit(std::move(job),{}));}catch(const std::exception& e){message=e.what();cyui::notify(e.what(),cyui::Tone::error);}
            cyui::endModal();
        }
    }
};
FileWorkspace::FileWorkspace(std::filesystem::path data,bool demo,bool split,bool prepare,Submit submit,Cancel cancel,std::function<void()> save):impl_(std::make_unique<Impl>(std::move(data),demo,split,prepare,std::move(submit),std::move(cancel),std::move(save))){}
FileWorkspace::~FileWorkspace()=default;
void FileWorkspace::tick(const std::string& d,uint64_t s,bool o,bool v){impl_->tick(d,s,o,v);}
std::optional<FileCompletion> FileWorkspace::event(const BackendEvent& e){return impl_->event(e);}
void FileWorkspace::draw(){impl_->draw();}
void FileWorkspace::browse(const std::string& directory){impl_->browse(false,directory);}
bool FileWorkspace::sideBySide()const{return impl_->split;}
bool FileWorkspace::prepareList()const{return impl_->prepare;}
bool FileWorkspace::busy()const{return impl_->queue.active()||impl_->queue.count(TransferState::waiting)!=0;}
nlohmann::json FileWorkspace::describe()const{const auto& p=*impl_;return {{"directory",p.remote.folder},{"selected",p.remote.selected.size()==1?*p.remote.selected.begin():std::string()},{"entries",p.remote.listing.entries.size()},{"loading",p.listingJob!=0},{"error",p.remote.error.substr(0,1000)},{"sideBySide",p.split},{"prepareList",p.prepare},{"localEntries",p.local.listing.entries.size()},{"selectedCount",p.remote.selected.size()},{"transfers",{{"waiting",p.queue.count(TransferState::waiting)},{"running",p.queue.active()},{"succeeded",p.queue.count(TransferState::succeeded)},{"failed",p.queue.count(TransferState::failed)},{"cancelled",p.queue.count(TransferState::cancelled)}}}};}
void FileWorkspace::sidebar(const std::vector<Package>& packages,const std::string& selectedPackage){
    auto& p=*impl_;cyui::overline("FICHIERS DU QUEST");cyui::space(10);
    const std::pair<const char*,const char*> shortcuts[]={{"Stockage partagé","/sdcard"},{"Téléchargements","/sdcard/Download"},{"Documents","/sdcard/Documents"},{"Images","/sdcard/Pictures"},{"Vidéos","/sdcard/Movies"},{"Données des applications","/sdcard/Android/data"},{"Extensions OBB","/sdcard/Android/obb"},{"Dossier temporaire ADB","/data/local/tmp"}};
    for(const auto& [name,path]:shortcuts)if(cyui::sidebarItem(cyui::icon::folder,name,p.remote.folder==path))p.browse(false,path);
    cyui::divider();cyui::textDim("Dossiers d’une application");cyui::searchField("##packageFiles",p.packageFilter.data(),int(p.packageFilter.size()),"Rechercher un package",-1);
    if(validPackage(selectedPackage)&&cyui::button("Application de la console"))p.browse(false,"/sdcard/Android/data/"+selectedPackage+"/files");
    if(p.packageFilter[0]){int count=0;for(const auto& app:packages)if(lower(app.name).find(lower(p.packageFilter.data()))!=std::string::npos&&count++<12){if(cyui::ghostButton((app.name+"###data"+app.name).c_str()))p.browse(false,"/sdcard/Android/data/"+app.name+"/files");}}
    cyui::divider();cyui::textFaint("%zu transfert(s) en attente\nCtrl / Maj : sélection multiple\nCtrl+A : fichiers du volet",p.queue.count(TransferState::waiting));
    if((p.queue.active()||p.queue.count(TransferState::waiting))&&cyui::button("Voir la liste"))p.showQueue=true;
    cyui::space(8);cyui::textFaint("Les droits Android s’appliquent.\nLes liens ne sont pas suivis.\nTransferts de fichiers réguliers.");
}
void FileWorkspace::seedQueueDemo(){auto& p=*impl_;if(!p.demo)return;p.split=p.prepare=p.showQueue=true;for(const auto& [name,direction]:std::vector<std::pair<std::string,TransferDirection>>{{"quest-demo.apk",TransferDirection::upload},{"capture de test.png",TransferDirection::download},{"session-équipe.json",TransferDirection::upload}}){FileTransfer f;f.direction=direction;f.device="DEMO-QUEST-3";f.remote="/sdcard/Download/"+name;f.local=localChild(L"C:\\QuestDev\\Workspace",name);f.size=name.find("apk")!=std::string::npos?137573171:524288;p.queue.add(std::move(f));}p.message="Liste préparée · données de démonstration";}
}
