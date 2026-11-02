// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/metrics_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace quest {
MetricPresentation prepareMetricPlot(const MetricsCsv& data,size_t column,double start,double end,size_t pixels){
    MetricPresentation result;pixels=std::clamp<size_t>(pixels,1,4096);result.statistics=metricsPlot(data,column,start,end,pixels);
    if(column>=data.columns.size()||!std::isfinite(start)||!std::isfinite(end)||end<=start)return result;
    auto begin=std::lower_bound(data.times.begin(),data.times.end(),start),finish=std::upper_bound(data.times.begin(),data.times.end(),end);
    if(size_t(finish-begin)<=pixels*4){for(auto it=begin;it!=finish;++it)result.points.push_back({*it,data.value(size_t(it-data.times.begin()),column)});return result;}
    // Keep the application's measured min/max envelope and missing-data policy.
    // CyUI's generic reducer conservatively omits a bucket containing a gap;
    // these counters must retain its measured spikes as isolated verticals.
    const auto gap=std::numeric_limits<double>::quiet_NaN();bool connected=false;size_t previousRow=0;
    for(size_t i=0;i<result.statistics.buckets.size();++i){const auto& b=result.statistics.buckets[i];double x=start+(double(i)+.5)/double(pixels)*(end-start);
        if(!b.count){if(b.missing){result.points.push_back({x,gap});connected=false;}continue;}
        if(!connected||b.missing||previousRow+1!=b.firstRow)result.points.push_back({x,gap});
        for(double value:{b.first,b.low,b.high,b.last})result.points.push_back({x,value});
        if(b.missing){result.points.push_back({x,gap});connected=false;}else connected=true;previousRow=b.lastRow;
    }
    return result;
}
}
