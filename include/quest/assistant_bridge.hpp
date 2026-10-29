// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <json.hpp>
#include <atomic>
#include <mutex>
#include <thread>
#include <deque>
#include <string>
namespace quest {
using Json=nlohmann::json;
class AssistantBridge {
public:
    ~AssistantBridge();
    bool start(const std::wstring& python,const std::wstring& script,const std::wstring& data,std::string& error);
    void stop();
    bool send(const Json& value);
    std::vector<Json> poll();
    bool running()const{return running_;}
private:
    void* process_{};void* input_{};void* output_{};void* job_{};
    std::thread reader_;std::atomic<bool> running_{false},stopping_{false};
    std::mutex mutex_,writeMutex_;std::deque<Json> events_;size_t eventBytes_{};
};
}
