// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/metrics_view.hpp"
#include "quest/backend.hpp"
#include "quest/metrics_presentation.hpp"
#include <cyui/Core.h>
#include <cyui/modules/Plots.h>
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
    struct Cache {uint64_t revision{};double start{},end{};size_t width{};MetricsPlot plot;cyui::PlotState view;std::vector<cyui::Point> points;bool autoY{true};};std::map<size_t,Cache> plots;
    Impl(std::filesystem::path path,bool simulation):monitor(path/L"cache/metrics",simulation),root(path/L"MetricsCaptures"),demo(simulation){}
    void request(const std::string& action,const std::filesystem::path& path={},const std::string& remote={}){try{pending.push_back({monitor.request(action,path,remote),action});}catch(const std::exception& e){cyui::notify(e.what(),cyui::Tone::error);}}
    void tick(){for(auto it=pending.begin();it!=pending.end();){if(it->first.result.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto result=it->first.result.get();if(!result.ok)cyui::notify(result.error.c_str(),cyui::Tone::error);it=pending.erase(it);}else ++it;}}
    void sync(const MetricsState& state){if(!state.data)return;std::string schema;for(const auto& column:state.data->columns)schema+=column+'\n';if(schema!=lastSchema){lastSchema=schema;selected.clear();for(auto id:{"average_frame_rate","app_gpu_time_microseconds","cpu_utilization_percentage","gpu_utilization_percentage"})if(std::find(state.data->columns.begin(),state.data->columns.end(),id)!=state.data->columns.end())selected.insert(id);if(selected.empty()&&!state.data->columns.empty())selected.insert(state.data->columns.front());autoRange=true;plots.clear();pinned=false;}auto duration=state.data->times.empty()?0:state.data->times.back();if(autoRange){start=0;end=std::max(.001,duration);}else{start=std::clamp(start,0.,std::max(0.,duration-.001));end=std::clamp(end,start+.001,std::max(start+.001,duration));}cursor=std::clamp(cursor,start,end);}
    void plot(const MetricsState& state,size_t column,float height){
        const auto& data=*state.data;const auto& title=data.columns[column];auto unit=metricsUnit(title);
        auto pixels=size_t(std::clamp(ImGui::GetContentRegionAvail().x,1.f,4096.f));auto& cache=plots[column];
        if(cache.revision!=state.dataRevision||cache.start!=start||cache.end!=end||cache.width!=pixels){
            auto prepared=prepareMetricPlot(data,column,start,end,pixels);cache.plot=std::move(prepared.statistics);cache.points=std::move(prepared.points);
            cache.revision=state.dataRevision;cache.start=start;cache.end=end;cache.width=pixels;
        }
        auto row=std::lower_bound(data.times.begin(),data.times.end(),cursor);size_t index=row==data.times.end()?data.times.size()-1:size_t(row-data.times.begin());
        if(index>0&&std::abs(data.times[index-1]-cursor)<std::abs(data.times[index]-cursor))--index;
        cyui::textColoured(cyui::categorical(int(column)),"%s",title.c_str());ImGui::SameLine();cyui::textDim("%s",number(data.value(index,column),unit).c_str());
        const auto& stats=cache.plot;double low=stats.count?stats.low:0,high=stats.count?stats.high:1;
        auto margin=high-low<1e-12?std::max(1.,std::abs(high)*.05):(high-low)*.08;
        if(cache.autoY)cache.view.y.range={low-margin,high+margin};cache.view.x.range={start,end};
        cyui::PlotOptions options;options.legend=false;options.formatX=[](double x){return number(x)+" s";};options.formatY=[unit](double y){return number(y,unit);};
        cyui::PlotSeries series{uint64_t(column)+1,title.c_str(),cache.points,cyui::PlotStyle::line,int(column)};
        // The shared time cursor is host selection state, represented by another
        // ordinary series rather than a second local canvas/grid implementation.
        cyui::Point marker[]={{cursor,cache.view.y.range.min},{cursor,cache.view.y.range.max}};
        cyui::PlotSeries all[]={series,{uint64_t(column)+1000,"",{marker,2},cyui::PlotStyle::line,0}};
        ImGui::PushID(int(column));auto hit=cyui::plot("##metric",cache.view,{all,pinned?size_t(2):size_t(1)},ImVec2(-1,std::max(cyui::dp(80),height-ImGui::GetTextLineHeightWithSpacing())),options);
        if(hit.viewChanged){auto duration=std::max(.001,data.times.back()),span=std::clamp(cache.view.x.range.max-cache.view.x.range.min,.001,duration);start=std::clamp(cache.view.x.range.min,0.,std::max(0.,duration-span));end=start+span;autoRange=false;if(ImGui::GetIO().KeyCtrl)cache.autoY=false;}
        if(hit.hovered){if(!pinned)cursor=std::clamp(hit.mouse.x,start,end);if(ImGui::IsMouseClicked(0)){pinned=true;cursor=std::clamp(hit.mouse.x,start,end);}if(ImGui::IsMouseDoubleClicked(0)){pinned=false;autoRange=true;for(auto& pair:plots)pair.second.autoY=true;}
            if(ImGui::BeginTooltip()){
                auto sample=std::lower_bound(data.times.begin(),data.times.end(),cursor);size_t at=sample==data.times.end()?data.times.size()-1:size_t(sample-data.times.begin());
                if(at>0&&std::abs(data.times[at-1]-cursor)<std::abs(data.times[at]-cursor))--at;
                ImGui::Text(cyui::tr("Sample: %.4f s").c_str(),data.times[at]);ImGui::Text(cyui::tr("Value: %s").c_str(),number(data.value(at,column),unit).c_str());
                if(stats.count)ImGui::Text("min %s · mean %s · max %s",number(stats.low,unit).c_str(),number(stats.average,unit).c_str(),number(stats.high,unit).c_str());else cyui::textFaint(cyui::tr("No available values in this range").c_str());ImGui::EndTooltip();}}
        if(!hit.skippedSeries.empty())cyui::callout(cyui::Tone::warning,cyui::tr("Plot point budget exceeded; reduce the selected interval.").c_str());
        ImGui::PopID();
    }
};
MetricsView::MetricsView(std::filesystem::path root,bool demo):impl_(std::make_unique<Impl>(std::move(root),demo)){}
MetricsView::~MetricsView()=default;
MetricsCapture& MetricsView::capture(){return impl_->monitor;}
void MetricsView::sidebar(){
    auto& v=*impl_;auto state=v.monitor.state();v.sync(state);cyui::overline(cyui::tr("OVR METRICS").c_str());cyui::space(10);
    if(cyui::button(cyui::trLabel("Open CSV…").c_str(),cyui::Variant::secondary,ImVec2(-1,0))){auto path=csvDialog(false);if(!path.empty())v.request("open_csv",path);}
    cyui::space(10);if(!state.data){cyui::textFaint(cyui::tr("The CSV columns will appear here.").c_str());return;}
    cyui::textDim("%zu métriques · %zu échantillons",state.data->columns.size(),state.data->times.size());cyui::searchField("##metricFilter",v.filter.data(),int(v.filter.size()),"Search metrics…");
    if(cyui::ghostButton(cyui::trLabel("Toutes").c_str()))v.selected={state.data->columns.begin(),state.data->columns.end()};ImGui::SameLine();if(cyui::ghostButton(cyui::trLabel("Aucune").c_str()))v.selected.clear();cyui::divider();
    auto query=lower(v.filter.data());for(size_t i=0;i<state.data->columns.size();++i){const auto& column=state.data->columns[i];if(lower(column).find(query)==std::string::npos)continue;bool enabled=v.selected.count(column)!=0;ImGui::PushID(int(i));if(ImGui::Checkbox("##select",&enabled)){if(enabled)v.selected.insert(column);else v.selected.erase(column);}ImGui::SameLine();ImGui::TextUnformatted(column.c_str());cyui::tooltip(column.c_str());ImGui::PopID();}
}
void MetricsView::draw(){
    auto& v=*impl_;v.tick();auto state=v.monitor.state();v.sync(state);bool busy=state.busy||!v.pending.empty();
    if(!state.online)v.refreshed=false;
    if(state.online&&!v.demo&&(!v.refreshed||v.refreshedSession!=state.deviceSession)){v.refreshed=true;v.refreshedSession=state.deviceSession;v.request("refresh_reports");busy=true;}
    cyui::panelHeader(cyui::tr("OVR curves and recordings").c_str(),cyui::tr("All CSV columns, shared time range, and sessions saved on PC.").c_str());cyui::space(10);
    {cyui::ScopedDisabled disabled(busy||state.recording||v.demo||!state.online);if(cyui::primaryButton(cyui::trLabel("Record on PC").c_str())){std::filesystem::create_directories(v.root);auto folder=v.root/(L"Session-"+std::to_wstring(GetTickCount64()));v.request("start_record",folder);}}
    ImGui::SameLine();{cyui::ScopedDisabled disabled(busy||!state.recording);if(cyui::button(cyui::trLabel("Stop").c_str()))v.request("stop_record");}
    ImGui::SameLine();if(cyui::button(cyui::trLabel("Open CSV…").c_str())){auto path=csvDialog(false);if(!path.empty())v.request("open_csv",path);}
    ImGui::SameLine();{cyui::ScopedDisabled disabled(!state.data||busy);if(cyui::button(cyui::trLabel("Export CSV…").c_str())){auto path=csvDialog(true);if(!path.empty())v.request("export_csv",path);}}
    ImGui::SameLine();if(cyui::ghostButton(cyui::trLabel("Session folder").c_str())){std::filesystem::create_directories(v.root);ShellExecuteW(nullptr,L"open",v.root.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
    ImGui::SameLine();if(cyui::ghostButton(cyui::trLabel("Configure OVR").c_str()))v.configureRequest=true;
    cyui::space(8);{cyui::ScopedDisabled disabled(busy||v.demo);if(cyui::iconButton(cyui::icon::refresh,"Actualiser les rapports OVR du Quest"))v.request("refresh_reports");}ImGui::SameLine();
    ImGui::SetNextItemWidth(cyui::dp(450));if(ImGui::BeginCombo("##remoteMetrics",state.remote.empty()?cyui::tr("CSV reports on Quest").c_str():state.remote.c_str())){for(const auto& report:state.reports){ImGui::PushID(report.path.c_str());if(ImGui::Selectable(report.name.c_str(),state.remote==report.path)){v.report=report.path;v.request("open_report",{},report.path);}cyui::tooltip(report.path.c_str());ImGui::PopID();}if(state.reports.empty())cyui::textFaint(cyui::tr("No reports. Refresh or start recording.").c_str());ImGui::EndCombo();}
    ImGui::SameLine();{cyui::ScopedDisabled disabled(busy||state.remote.empty()||v.demo);bool follow=state.follow;if(cyui::toggleSwitch(cyui::trLabel("Follow CSV writes").c_str(),&follow))v.request(follow?"follow_report":"pause",{},state.remote);}
    if(state.recording){cyui::space(6);cyui::badge("ENREGISTREMENT",cyui::Tone::error);ImGui::SameLine();cyui::textDim("%zu rapport(s) · %s",state.savedFiles,state.ownsCsv?"CSV activé par QuestDev":"Mode CSV déjà actif, conservé à l’arrêt");}
    if(busy)cyui::progressBar(-1,ImVec2(-1,cyui::dp(4)));if(!state.error.empty())cyui::callout(cyui::Tone::error,state.error.c_str());else if(!state.status.empty())cyui::textDim("%s",state.status.c_str());
    if(!state.diagnostic.empty())cyui::callout(cyui::Tone::warning,state.diagnostic.c_str());
    if(!state.reportDirectories.empty()){cyui::textFaint("%zu dossier(s) OVR détecté(s) · rapports des sous-dossiers inclus",state.reportDirectories.size());cyui::tooltip([&]{std::string text;for(const auto& directory:state.reportDirectories)text+=directory+'\n';return text;}().c_str());}
    if(!state.data||state.data->times.empty()){cyui::space(20);cyui::emptyState(cyui::tr("No samples to plot").c_str(),cyui::tr("Ouvrez un CSV OVR ou enregistrez une session pendant que votre application VR tourne. QuestDev ne lance pas de jeu.").c_str(),cyui::icon::clock);return;}
    cyui::space(6);cyui::textDim("%s · %zu lignes · %.3f s",state.data->name.c_str(),state.data->times.size(),state.data->times.back());cyui::textFaint("Temps relatif au premier timestamp CSV · molette : zoom · bouton central : déplacer");
    if(cyui::iconButton(cyui::icon::zoomIn,"Afficher toute la session (double clic sur une courbe)")){v.autoRange=true;for(auto& pair:v.plots)pair.second.autoY=true;}ImGui::SameLine();if(cyui::ghostButton(cyui::trLabel(v.pinned?"Release cursor":"Free cursor").c_str()))v.pinned=false;ImGui::SameLine();cyui::textMono("Curseur %.3f s · plage %.3f → %.3f s",v.cursor,v.start,v.end);
    std::vector<size_t> columns;for(size_t i=0;i<state.data->columns.size();++i)if(v.selected.count(state.data->columns[i]))columns.push_back(i);
    if(columns.empty()){cyui::emptyState(cyui::tr("No curves selected").c_str(),cyui::tr("Select metrics on the left.").c_str(),cyui::icon::filter);return;}
    ImGui::BeginChild("##metricsPlots");float height=cyui::dp(148),spacing=cyui::dp(8);ImGuiListClipper clipper;clipper.Begin(int(columns.size()),height+spacing+2*ImGui::GetStyle().ItemSpacing.y);while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){v.plot(state,columns[size_t(i)],height);ImGui::Dummy(ImVec2(0,spacing));}ImGui::EndChild();
}
nlohmann::json MetricsView::describe()const{auto result=impl_->monitor.describe();result["selectedMetrics"]=impl_->selected;result["range"]={{"start",impl_->start},{"end",impl_->end}};result["cursor"]=impl_->cursor;return result;}
bool MetricsView::takeConfigureRequest(){bool value=impl_->configureRequest;impl_->configureRequest=false;return value;}
}
