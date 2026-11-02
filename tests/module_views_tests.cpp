// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/metrics_presentation.hpp"
#include <cyui/modules/Images.h>
#include <cyui/modules/DocumentViews.h>
#include <imgui_internal.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace quest {void registerUiTranslations();}
static int checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
int main(){try{
    std::string csv="timestamp,value\n";for(int i=0;i<20000;++i)csv+=std::to_string(i)+","+(i==10000?"10000":i==10001?"":i==18000?"-100":"1")+"\n";
    auto data=quest::parseMetricsCsv(csv);auto reduced=quest::prepareMetricPlot(data,0,0,20,16);
    bool spike=false,gap=false,negative=false;for(const auto& point:reduced.points){spike|=point.y==10000;negative|=point.y==-100;gap|=!std::isfinite(point.y);}
    check(reduced.points.size()<=16*6&&spike&&gap&&negative,"OVR envelope keeps spikes beside missing samples within CyUI's point budget");
    check(reduced.statistics.high==10000&&reduced.statistics.low==-100&&reduced.statistics.count==19999,"display reduction does not alter measured statistics");
    auto exact=quest::prepareMetricPlot(data,0,0,.01,16);check(exact.points.size()==11&&exact.points[1].x==.001,"short ranges keep original timestamps and samples");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={800,600};io.DeltaTime=1.f/60;
    cyui::Config config;config.density=cyui::Density::compact;cyui::apply(config);quest::registerUiTranslations();
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    cyui::PlotState plot{{{0,10},false},{{0,5},false}};cyui::Point samples[]={{0,1},{10,3}};cyui::PlotSeries series{9,"Counter",{samples,2}};cyui::PlotResult result;
    cyui::ImageState image;cyui::ImageData texture{1,64,64};cyui::TranscriptState transcript;std::vector<cyui::TranscriptMessage> messages={{0x100000001ULL,"You","Original **UTF-8** content: é",{}},{0x200000001ULL,"Assistant","```json\n{\"fps\":90}\n```",{}}};
    int mode=0;ImGuiID english{},french{};
    auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({800,600});ImGui::Begin("host",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
        cyui::setLocale("en");english=ImGui::GetID(cyui::trLabel("Settings").c_str());cyui::setLocale("fr");french=ImGui::GetID(cyui::trLabel("Settings").c_str());
        if(mode==0)result=cyui::plot("counter",plot,{&series,1},{760,500});else if(mode==1)cyui::imageViewport("image",image,texture,{760,500});else cyui::transcript("conversation",transcript,messages,{760,500});
        ImGui::End();ImGui::Render();};
    io.AddMousePosEvent(300,200);frame();frame();io.AddMouseWheelEvent(0,1);frame();
    check(result.viewChanged&&plot.x.range.span()<10,"common plot handles anchored wheel zoom");
    check(english==french&&cyui::tr("Settings")=="Réglages","language changes preserve control IDs and use the host catalog");
    mode=1;frame();frame();auto zoom=image.canvas.view.zoom;io.AddMouseWheelEvent(0,1);frame();check(image.canvas.view.zoom>zoom,"common screenshot viewport handles zoom");
    mode=2;frame();frame();check(transcript.count==2&&transcript.documents.size()==2,"conversation retains full 64-bit message identities");
    messages[1].content+="\n\nStreamed **update**.";frame();check(transcript.documents.at(messages[1].id).blocks().size()>=2,"streaming updates refresh the common Markdown model");
    messages.erase(messages.begin());frame();check(transcript.documents.size()==1&&!transcript.documents.count(0x100000001ULL),"removed messages release presentation caches");
    auto content=messages[0].content;cyui::applyDpi(1.5f);frame();check(messages[0].content==content&&transcript.count==1,"DPI changes do not mutate conversation content or IDs");
    ImGui::DestroyContext();std::cout<<checks<<" module integration checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<"\n";return 1;}}
