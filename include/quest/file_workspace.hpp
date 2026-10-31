// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "quest/transfers.hpp"
#include "quest/plugins.hpp"
#include <json.hpp>
#include <functional>
#include <memory>

namespace quest {
struct FileCompletion {bool ok{};std::string message;};
class FileWorkspace {
public:
    using Submit=std::function<uint64_t(JobRequest,const std::wstring&)>;
    using Cancel=std::function<void(uint64_t)>;
    FileWorkspace(std::filesystem::path data,bool demo,bool split,bool prepare,Submit submit,Cancel cancel,std::function<void()> save);
    ~FileWorkspace();
    void tick(const std::string& device,uint64_t session,bool online,bool visible);
    std::optional<FileCompletion> event(const BackendEvent& event);
    void draw();
    void sidebar(const std::vector<Package>& packages,const std::string& selectedPackage={});
    void browse(const std::string& directory);
    nlohmann::json describe() const;
    bool sideBySide() const;
    bool prepareList() const;
    bool busy() const;
    void seedQueueDemo();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
