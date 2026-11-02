// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/metrics_view.hpp"
#include "quest/backend.hpp"
#include <cyui/CyUI.h>
#include <imgui.h>
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

namespace quest {
namespace {
std::filesystem::path csvDialog(bool save){wchar_t path[32768]{};if(save)wcscpy_s(path,L"session.csv");OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFilter=L"OVR Metrics CSV (*.csv)\0*.csv\0\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrDefExt=L"csv";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);return (save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))?path:L"";}
std::string number(double value,const std::string& unit={}){if(!std::isfinite(value))return "Indisponible";char buffer[96];std::snprintf(buffer,sizeof(buffer),"%.4g%s%s",value,unit.empty()?"":" ",unit.c_str());return buffer;}
}
struct MetricsView::Impl {
    MetricsCapture monitor;std::filesystem::path root;bool demo{},configureRequest{},refreshed{};uint64_t refreshedSession{};
    std::set<std::string> selected;std::array<char,256> filter{};std::string lastSchema,report;
    std::vector<std::pair<MetricsTicket,std::string>> pending;
    double start{},end{1},cursor{};bool autoRange{true},pinned{};
    struct Cache {uint64_t revision{};double start{},end{};size_t width{};MetricsPlot plot;};std::map<size_t,Cache> plots;
    Impl(std::filesystem::path path,bool simulation):monitor(path/L"cache/metrics",simulation),root(path/L"MetricsCaptures"),demo(simulation){}
    void request(const std::string& action,const std::filesystem::path& path={},const std::string& remote={}){try{pending.push_back({monitor.request(action,path,remote),action});}catch(const std::exception& e){cyui::notify(e.what(),cyui::Tone::error);}}
    void tick(){for(auto it=pending.begin();it!=pending.end();){if(it->first.result.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=it->first.result.get();if(!result.ok)cyui::notify(result.error.c_str(),cyui::Tone::error);it=pending.erase(it);}else ++it;}}
    void sync(const MetricsState& state){if(!state.data)return;std::string schema;for(const auto& column:state.data->columns)schema+=column+'\n';if(schema!=lastSchema){lastSchema=schema;selected.clear();for(auto id:{"average_frame_rate","app_gpu_time_microseconds","cpu_utilization_percentage","gpu_utilization_percentage"})if(std::find(state.data->columns.begin(),state.data->columns.end(),id)!=state.data->columns.end())selected.insert(id);if(selected.empty()&&!state.data->columns.empty())selected.insert(state.data->columns.front());autoRange=true;plots.clear();pinned=false;}auto duration=state.data->times.empty()?0:state.data->times.back();if(autoRange){start=0;end=std::max(.001,duration);}else{start=std::clamp(start,0.,std::max(0.,duration-.001));end=std::clamp(end,start+.001,std::max(start+.001,duration));}cursor=std::clamp(cursor,start,end);}
    void plot(const MetricsState& state,size_t column,float height){
        const auto& data=*state.data;auto& style=ImGui::GetStyle();float width=ImGui::GetContentRegionAvail().x;auto position=ImGui::GetCursorScreenPos();float labelWidth=cyui::dp(68);ImVec2 low(position.x+labelWidth,position.y+cyui::dp(28)),high(position.x+width-cyui::dp(12),position.y+height-cyui::dp(25));float plotWidth=std::max(1.f,high.x-low.x);
        ImGui::PushID(int(column));ImGui::InvisibleButton("##curve",ImVec2(width,height),ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle);bool hovered=ImGui::IsItemHovered();auto& io=ImGui::GetIO();
        if(hovered){double under=start+std::clamp(double((io.MousePos.x-low.x)/plotWidth),0.,1.)*(end-start);if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)){pinned=true;cursor=under;}if(!pinned)cursor=under;
            if(io.MouseWheel!=0){double duration=std::max(.001,data.times.empty()?1:data.times.back()),span=std::clamp((end-start)*std::pow(.8,io.MouseWheel),.001,duration),ratio=(under-start)/(end-start);start=std::clamp(under-ratio*span,0.,std::max(0.,duration-span));end=start+span;autoRange=false;}
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Middle)){double duration=std::max(.001,data.times.empty()?1:data.times.back()),span=end-start;start=std::clamp(start-double(io.MouseDelta.x)/plotWidth*span,0.,std::max(0.,duration-span));end=start+span;autoRange=false;}
            if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){pinned=false;autoRange=true;}
        }
        auto pixels=size_t(std::clamp(plotWidth,1.f,4096.f));auto& cache=plots[column];if(cache.revision!=state.dataRevision||cache.start!=start||cache.end!=end||cache.width!=pixels){cache.plot=metricsPlot(data,column,start,end,pixels);cache.revision=state.dataRevision;cache.start=start;cache.end=end;cache.width=pixels;}
        const auto& series=cache.plot;const auto& palette=cyui::palette();auto* draw=ImGui::GetWindowDrawList();auto colour=cyui::categorical(int(column));draw->AddRectFilled(position,ImVec2(position.x+width,position.y+height),palette.bg0,cyui::dp(4));draw->AddRect(position,ImVec2(position.x+width,position.y+height),palette.line,cyui::dp(4));
        auto title=data.columns[column];auto unit=metricsUnit(title);draw->AddText(ImVec2(position.x+cyui::dp(10),position.y+cyui::dp(6)),colour,title.c_str());
        auto row=std::lower_bound(data.times.begin(),data.times.end(),cursor);size_t index=row==data.times.end()?data.times.empty()?0:data.times.size()-1:size_t(row-data.times.begin());if(index>0&&std::abs(data.times[index-1]-cursor)<std::abs(data.times[index]-cursor))--index;
        double value=data.times.empty()?NAN:data.value(index,column);std::string summary=number(value,unit);auto labelSize=ImGui::CalcTextSize(summary.c_str());draw->AddText(ImVec2(high.x-labelSize.x,position.y+cyui::dp(6)),palette.text,summary.c_str());
        double bottom=series.low,top=series.high;if(!series.count){draw->AddText(ImVec2(low.x+cyui::dp(12),low.y+cyui::dp(18)),palette.textFaint,"Aucune valeur disponible dans cette plage");bottom=0;top=1;}else if(top-bottom<1e-12){double margin=std::max(1.,std::abs(top)*.05);bottom-=margin;top+=margin;}else{auto margin=(top-bottom)*.08;bottom-=margin;top+=margin;}
        auto y=[&](double v){return high.y-float((v-bottom)/(top-bottom))*(high.y-low.y);};
        for(int i=0;i<4;++i){float x=low.x+(high.x-low.x)*float(i)/3;draw->AddLine(ImVec2(x,low.y),ImVec2(x,high.y),palette.grid);}
        for(int i=0;i<3;++i){double v=bottom+(top-bottom)*i/2;float yy=y(v);draw->AddLine(ImVec2(low.x,yy),ImVec2(high.x,yy),palette.grid);auto text=number(v);draw->AddText(ImVec2(position.x+cyui::dp(6),yy-ImGui::GetFontSize()*.5f),palette.textFaint,text.c_str());}
        draw->PushClipRect(low,high,true);const MetricsBucket* previous=nullptr;float previousX=0;
        for(size_t i=0;i<series.buckets.size();++i){const auto& bucket=series.buckets[i];if(!bucket.count){if(bucket.missing)previous=nullptr;continue;}float x=low.x+(float(i)+.5f)/float(pixels)*plotWidth;draw->AddLine(ImVec2(x,y(bucket.low)),ImVec2(x,y(bucket.high)),colour,cyui::dp(1.2f));if(previous&&!previous->missing&&!bucket.missing&&previous->lastRow+1==bucket.firstRow)draw->AddLine(ImVec2(previousX,y(previous->last)),ImVec2(x,y(bucket.first)),colour,cyui::dp(1.3f));else draw->AddCircleFilled(ImVec2(x,y(bucket.last)),cyui::dp(1.3f),colour);previous=&bucket;previousX=x;}
        float cx=low.x+float((cursor-start)/(end-start))*plotWidth;draw->AddLine(ImVec2(cx,low.y),ImVec2(cx,high.y),palette.accent);draw->PopClipRect();
        auto left=number(start)+" s",right=number(end)+" s";draw->AddText(ImVec2(low.x,high.y+cyui::dp(4)),palette.textFaint,left.c_str());draw->AddText(ImVec2(high.x-ImGui::CalcTextSize(right.c_str()).x,high.y+cyui::dp(4)),palette.textFaint,right.c_str());
        if(hovered&&ImGui::BeginTooltip()){ImGui::Text("Échantillon : %.4f s",data.times.empty()?0:data.times[index]);ImGui::Text("Valeur : %s",summary.c_str());if(series.count)ImGui::Text("Plage : min %s · moy %s · max %s",number(series.low).c_str(),number(series.average).c_str(),number(series.high).c_str());else cyui::textFaint("Statistiques indisponibles");cyui::textFaint("Molette : zoom · bouton central : déplacer\nClic : fixer le curseur · double clic : vue entière");ImGui::EndTooltip();}
        ImGui::PopID();(void)style;
    }
};
MetricsView::MetricsView(std::filesystem::path root,bool demo):impl_(std::make_unique<Impl>(std::move(root),demo)){}
MetricsView::~MetricsView()=default;
MetricsCapture& MetricsView::capture(){return impl_->monitor;}
void MetricsView::sidebar(){
    auto& v=*impl_;auto state=v.monitor.state();v.sync(state);cyui::overline("MÉTRIQUES OVR");cyui::space(10);
    if(cyui::button("Ouvrir un CSV…",cyui::Variant::secondary,ImVec2(-1,0))){auto path=csvDialog(false);if(!path.empty())v.request("open_csv",path);}
    cyui::space(10);if(!state.data){cyui::textFaint("Les colonnes du CSV apparaîtront ici.");return;}
    cyui::textDim("%zu métriques · %zu échantillons",state.data->columns.size(),state.data->times.size());cyui::searchField("##metricFilter",v.filter.data(),int(v.filter.size()),"Chercher une métrique…");
    if(cyui::ghostButton("Toutes"))v.selected={state.data->columns.begin(),state.data->columns.end()};ImGui::SameLine();if(cyui::ghostButton("Aucune"))v.selected.clear();cyui::divider();
    auto query=lower(v.filter.data());for(size_t i=0;i<state.data->columns.size();++i){const auto& column=state.data->columns[i];if(lower(column).find(query)==std::string::npos)continue;bool enabled=v.selected.count(column)!=0;ImGui::PushID(int(i));if(ImGui::Checkbox("##select",&enabled)){if(enabled)v.selected.insert(column);else v.selected.erase(column);}ImGui::SameLine();ImGui::TextUnformatted(column.c_str());cyui::tooltip(column.c_str());ImGui::PopID();}
}
void MetricsView::draw(){
    auto& v=*impl_;v.tick();auto state=v.monitor.state();v.sync(state);bool busy=state.busy||!v.pending.empty();
    if(!state.online)v.refreshed=false;
    if(state.online&&!v.demo&&(!v.refreshed||v.refreshedSession!=state.deviceSession)){v.refreshed=true;v.refreshedSession=state.deviceSession;v.request("refresh_reports");busy=true;}
    cyui::panelHeader("Courbes et enregistrements OVR","Toutes les colonnes disponibles du CSV, une base de temps commune et des sessions sauvegardées sur PC.");cyui::space(10);
    {cyui::ScopedDisabled disabled(busy||state.recording||v.demo||!state.online);if(cyui::primaryButton("Enregistrer sur PC")){std::filesystem::create_directories(v.root);auto folder=v.root/(L"Session-"+std::to_wstring(GetTickCount64()));v.request("start_record",folder);}}
    ImGui::SameLine();{cyui::ScopedDisabled disabled(busy||!state.recording);if(cyui::button("Arrêter"))v.request("stop_record");}
    ImGui::SameLine();if(cyui::button("Ouvrir un CSV…")){auto path=csvDialog(false);if(!path.empty())v.request("open_csv",path);}
    ImGui::SameLine();{cyui::ScopedDisabled disabled(!state.data||busy);if(cyui::button("Exporter le CSV…")){auto path=csvDialog(true);if(!path.empty())v.request("export_csv",path);}}
    ImGui::SameLine();if(cyui::ghostButton("Dossier des sessions")){std::filesystem::create_directories(v.root);ShellExecuteW(nullptr,L"open",v.root.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
    ImGui::SameLine();if(cyui::ghostButton("Configurer OVR"))v.configureRequest=true;
    cyui::space(8);{cyui::ScopedDisabled disabled(busy||v.demo);if(cyui::iconButton(cyui::icon::refresh,"Actualiser les rapports OVR du Quest"))v.request("refresh_reports");}ImGui::SameLine();
    ImGui::SetNextItemWidth(cyui::dp(450));if(ImGui::BeginCombo("##remoteMetrics",state.remote.empty()?"Rapports CSV sur le Quest":state.remote.c_str())){for(const auto& report:state.reports){ImGui::PushID(report.path.c_str());if(ImGui::Selectable(report.name.c_str(),state.remote==report.path)){v.report=report.path;v.request("open_report",{},report.path);}cyui::tooltip(report.path.c_str());ImGui::PopID();}if(state.reports.empty())cyui::textFaint("Aucun rapport. Actualisez ou démarrez un enregistrement.");ImGui::EndCombo();}
    ImGui::SameLine();{cyui::ScopedDisabled disabled(busy||state.remote.empty()||v.demo);bool follow=state.follow;if(cyui::toggleSwitch("Suivre les écritures CSV",&follow))v.request(follow?"follow_report":"pause",{},state.remote);}
    if(state.recording){cyui::space(6);cyui::badge("ENREGISTREMENT",cyui::Tone::error);ImGui::SameLine();cyui::textDim("%zu rapport(s) · %s",state.savedFiles,state.ownsCsv?"CSV activé par QuestDev":"Mode CSV déjà actif, conservé à l’arrêt");}
    if(busy)cyui::progressBar(-1,ImVec2(-1,cyui::dp(4)));if(!state.error.empty())cyui::callout(cyui::Tone::error,state.error.c_str());else if(!state.status.empty())cyui::textDim("%s",state.status.c_str());
    if(!state.diagnostic.empty())cyui::callout(cyui::Tone::warning,state.diagnostic.c_str());
    if(!state.reportDirectories.empty()){cyui::textFaint("%zu dossier(s) OVR détecté(s) · rapports des sous-dossiers inclus",state.reportDirectories.size());cyui::tooltip([&]{std::string text;for(const auto& directory:state.reportDirectories)text+=directory+'\n';return text;}().c_str());}
    if(!state.data||state.data->times.empty()){cyui::space(20);cyui::emptyState("Aucun échantillon à tracer","Ouvrez un CSV OVR ou enregistrez une session pendant que votre application VR tourne. QuestDev ne lance pas de jeu.",cyui::icon::clock);return;}
    cyui::space(6);cyui::textDim("%s · %zu lignes · %.3f s",state.data->name.c_str(),state.data->times.size(),state.data->times.back());cyui::textFaint("Temps relatif au premier timestamp CSV · molette : zoom · bouton central : déplacer");
    if(cyui::iconButton(cyui::icon::zoomIn,"Afficher toute la session (double clic sur une courbe)"))v.autoRange=true;ImGui::SameLine();if(cyui::ghostButton(v.pinned?"Libérer le curseur":"Curseur libre"))v.pinned=false;ImGui::SameLine();cyui::textMono("Curseur %.3f s · plage %.3f → %.3f s",v.cursor,v.start,v.end);
    std::vector<size_t> columns;for(size_t i=0;i<state.data->columns.size();++i)if(v.selected.count(state.data->columns[i]))columns.push_back(i);
    if(columns.empty()){cyui::emptyState("Aucune courbe sélectionnée","Cochez les métriques à gauche.",cyui::icon::filter);return;}
    ImGui::BeginChild("##metricsPlots");float height=cyui::dp(148),spacing=cyui::dp(8);ImGuiListClipper clipper;clipper.Begin(int(columns.size()),height+spacing+2*ImGui::GetStyle().ItemSpacing.y);while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){v.plot(state,columns[size_t(i)],height);ImGui::Dummy(ImVec2(0,spacing));}ImGui::EndChild();
}
nlohmann::json MetricsView::describe()const{auto result=impl_->monitor.describe();result["selectedMetrics"]=impl_->selected;result["range"]={{"start",impl_->start},{"end",impl_->end}};result["cursor"]=impl_->cursor;return result;}
bool MetricsView::takeConfigureRequest(){bool value=impl_->configureRequest;impl_->configureRequest=false;return value;}
}
