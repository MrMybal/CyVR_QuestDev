// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "quest/backend.hpp"
#include <optional>

namespace quest {
struct RemoteFile {
    std::string name,target;
    char type{'o'};
    std::optional<uint64_t> size;
    std::optional<int64_t> modified;
    bool hidden{};
};
struct RemoteListing {std::vector<RemoteFile> entries;bool truncated{};};
std::string remotePath(const std::string& path);
std::string remoteParent(const std::string& path);
std::string remoteChild(const std::string& parent,const std::string& name);
bool validRemoteName(const std::string& name);
RemoteListing parseFileListing(const std::string& bytes);
std::string fileListingScript(const std::string& path);
JobRequest listRemoteFiles(const std::string& path,const std::wstring& output);
JobRequest pullRemoteFile(const std::string& source,const std::wstring& destination);
JobRequest pushRemoteFile(const std::wstring& source,const std::string& destination,bool overwrite=false);
JobRequest changeRemoteFile(const std::string& action,const std::string& path,const std::string& name={});
}
