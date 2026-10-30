// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "quest/backend.hpp"
#include <json.hpp>
#include <future>
#include <filesystem>

namespace quest {
struct MirrorOptions {
    int maxSize{1920},maxFps{60},bitRateMbps{12},audio{1}; // 0 off, 1 duplicate, 2 PC output
    bool fullscreen{},alwaysOnTop{},overwriteRecording{};
    int cropWidth{},cropHeight{},cropX{},cropY{};
    std::filesystem::path recording;
};
std::vector<std::string> mirrorArguments(const std::string& serial,const std::string& title,const MirrorOptions& options);
struct MirrorState {
    bool active{},starting{},stopping{},recording{},forcedStop{};
    std::string serial,title,status,error,log,audioStatus{"off"};
    int width{},height{},fps{},exitCode{};
    uint32_t processId{};
    std::filesystem::path recordingFile;
    MirrorOptions options;
};
struct MirrorResult {bool ok{};std::string error;nlohmann::json data=nlohmann::json::object();};
struct MirrorTicket {uint64_t id{};std::shared_future<MirrorResult> result;};
class Mirroring {
public:
    Mirroring();~Mirroring();
    MirrorTicket start(const std::wstring& engine,const std::wstring& adb,const std::string& serial,const MirrorOptions& options);
    MirrorTicket stop();
    void cancel(uint64_t id);
    void focus();
    MirrorState state()const;
    nlohmann::json describe()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
