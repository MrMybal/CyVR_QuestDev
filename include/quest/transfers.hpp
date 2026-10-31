// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "quest/files.hpp"
#include <filesystem>

namespace quest {
enum class TransferDirection { upload, download };
enum class TransferState { waiting, running, succeeded, failed, cancelled };
struct FileTransfer {
    uint64_t id{},job{};
    TransferDirection direction{};
    TransferState state{TransferState::waiting};
    std::string device,remote,error;
    std::wstring local;
    bool overwrite{},cancelRequested{};
    std::optional<uint64_t> size;
};
// Session-only preparation: nothing is dispatched until start() is explicit.
class TransferQueue {
public:
    uint64_t add(FileTransfer item);
    void start(const std::string& device);
    const FileTransfer* next() const;
    void started(uint64_t item,uint64_t job);
    bool finish(uint64_t job,bool ok,bool cancelled,const std::string& message);
    void reject(uint64_t item,const std::string& message);
    uint64_t cancel(uint64_t item);
    uint64_t cancelAll();
    uint64_t pause();
    void retry(bool includeCancelled=false);
    void clearFinished();
    bool running() const { return running_; }
    bool active() const;
    size_t count(TransferState state) const;
    const std::vector<FileTransfer>& items() const { return items_; }
    bool stopOnError{true};
private:
    std::vector<FileTransfer> items_;
    uint64_t nextId_{1};
    bool running_{};
};
struct TransferPlan {JobRequest request;std::wstring temporary,destination;bool overwrite{};};
TransferPlan prepareTransfer(const FileTransfer& item);
void finishTransfer(const TransferPlan& plan,bool success);
std::wstring localChild(const std::wstring& folder,const std::string& name);
RemoteListing listLocalFiles(const std::wstring& folder);
}
