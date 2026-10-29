#pragma once
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace quest {
struct MetricsCsv {
    std::string name,raw;
    std::vector<std::string> columns;
    std::vector<double> times,values;
    double firstTimestamp{};
    size_t incompleteRows{};
    double value(size_t row,size_t column)const{return values[row*columns.size()+column];}
};
MetricsCsv parseMetricsCsv(const std::string& bytes,bool growing=false,const std::atomic<bool>* cancelled=nullptr);
MetricsCsv readMetricsCsv(const std::filesystem::path& path,bool growing=false,const std::atomic<bool>* cancelled=nullptr);
std::string metricsUnit(const std::string& column);
struct MetricsBucket {double low{},high{},first{},last{};size_t count{},firstRow{},lastRow{};bool missing{};};
struct MetricsPlot {std::vector<MetricsBucket> buckets;double low{},high{},average{};size_t count{};};
MetricsPlot metricsPlot(const MetricsCsv& data,size_t column,double start,double end,size_t pixels);
std::string demoMetricsCsv();
}
