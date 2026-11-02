// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace quest {
struct ProcessInfo { int pid{}, uid{}; std::string name; };
struct Device { std::string serial, state, model; };
struct Package { std::string name; int uid{}; bool system{}; };
struct LogEntry {
    uint64_t id{};
    double epoch{};
    int pid{}, tid{};
    char level{'I'};
    std::string tag, message, process, package, search, raw;
    bool lifecycle{};
};
std::string trim(std::string s);
std::string lower(std::string s);
bool validPackage(const std::string& s);
int severity(char level);
std::vector<Device> parseDevices(const std::string& text);
std::vector<Package> parsePackages(const std::string& text, bool system = false);
std::vector<ProcessInfo> parseProcesses(const std::string& text);
std::optional<LogEntry> parseLog(const std::string& line);
std::string formatLog(const LogEntry& entry);
std::string rawLog(const LogEntry& entry);
enum class LogColumn { time, level, pid, application, tag, message };
struct LogSort { LogColumn column{LogColumn::time}; bool descending{}; };
bool logLess(const LogEntry& a, const LogEntry& b, const std::vector<LogSort>& order);
// Observe the viewport before adding rows: its maximum still describes the
// previous frame, so incoming lines cannot be mistaken for a manual scroll.
bool logFollow(bool following,float previous,float current,float maximum,bool scrollUp,float tolerance);
std::string exportLogs(const std::vector<const LogEntry*>& entries, bool jsonl);

struct LogFilter {
    std::set<std::string> packages;
    std::string text, exclude, tag;
    int minimum{}, pid{};
    bool onlyUnassigned{}, hideLifecycle{true};
    bool matches(const LogEntry& e) const;
};

// Timestamped process lifetimes prevent PID reuse from moving old records.
class LogStore {
public:
    explicit LogStore(size_t capacity = 50000, size_t bytes = 64 * 1024 * 1024);
    void setPackages(const std::vector<Package>& packages);
    void snapshot(const std::vector<ProcessInfo>& processes, double epoch);
    bool ingest(const std::string& line);
    void clear();
    void reset();
    void setLimits(size_t capacity, size_t bytes);
    size_t capacity() const { return capacity_; }
    size_t byteLimit() const { return byteLimit_; }
    const std::deque<LogEntry>& entries() const { return entries_; }
    const std::vector<ProcessInfo>& processes() const { return processes_; }
    std::string packageFor(const std::string& process) const;
    uint64_t revision() const { return revision_; }
    uint64_t discarded() const { return discarded_; }
    size_t bytes() const { return bytes_; }
private:
    struct Lifetime { double start{}, end{}; std::string process; };
    std::map<int, std::vector<Lifetime>> lives_;
    std::set<std::string> packages_;
    std::vector<ProcessInfo> processes_;
    std::deque<LogEntry> entries_;
    size_t capacity_, byteLimit_, bytes_{};
    uint64_t nextId_{1}, revision_{}, discarded_{};
    void start(int pid, const std::string& name, double epoch);
    void end(int pid, double epoch);
    void attribute(LogEntry& e) const;
    void reattribute(int pid);
    void limit();
};
}
