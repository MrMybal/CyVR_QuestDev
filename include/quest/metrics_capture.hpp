// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "quest/metrics_data.hpp"
#include <json.hpp>
#include <future>
#include <memory>

namespace quest {
struct MetricsReport {std::string path,name;uint64_t size{};int64_t modified{};};
struct MetricsReportListing {std::vector<MetricsReport> reports;std::vector<std::string> directories,denied;};
std::string metricsReportScript();
MetricsReportListing parseMetricsReports(const std::string& text);
struct MetricsState {
    std::shared_ptr<const MetricsCsv> data;
    std::vector<MetricsReport> reports;
    std::string error,status,remote,sourceDevice,recordingId,diagnostic;
    std::vector<std::string> reportDirectories;
    std::filesystem::path recordingFolder;
    uint64_t dataRevision{},deviceSession{};size_t savedFiles{};
    bool busy{},recording{},follow{},ownsCsv{},online{};
};
struct MetricsResult {bool ok{};nlohmann::json data=nlohmann::json::object();std::string error;};
struct MetricsTicket {uint64_t id{};std::shared_future<MetricsResult> result;};
class MetricsCapture {
public:
    explicit MetricsCapture(std::filesystem::path cache,bool demo=false);
    ~MetricsCapture();
    void device(std::wstring adb,std::string serial,uint64_t session,bool online);
    MetricsState state()const;
    MetricsTicket request(const std::string& action,const std::filesystem::path& path={},const std::string& remote={},int seconds=0);
    void cancel(uint64_t ticket);
    nlohmann::json describe()const;
    nlohmann::json query(const nlohmann::json& parameters)const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
