// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/core.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace quest {
static size_t cost(const LogEntry& e);
std::string trim(std::string s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::string lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
bool validPackage(const std::string& s) {
    return !s.empty() && s.front() != '-' && s.find('.') != std::string::npos &&
        std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isalnum(c) || c == '.' || c == '_'; });
}
int severity(char l) { const std::string levels = "VDIWEF"; auto p = levels.find(l); return p == std::string::npos ? 2 : static_cast<int>(p); }
std::vector<Device> parseDevices(const std::string& text) {
    std::vector<Device> out; std::istringstream lines(text); std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line.find("List of devices") != std::string::npos || line[0] == '*') continue;
        std::istringstream s(line); Device d; s >> d.serial >> d.state;
        if (d.state != "device" && d.state != "offline" && d.state != "unauthorized" && d.state != "recovery" && d.state != "sideload") continue;
        std::string field; while (s >> field) if (field.rfind("model:", 0) == 0) d.model = field.substr(6);
        std::replace(d.model.begin(), d.model.end(), '_', ' '); out.push_back(d);
    }
    return out;
}
std::vector<Package> parsePackages(const std::string& text, bool system) {
    std::vector<Package> out; std::istringstream lines(text); std::string line;
    while (std::getline(lines, line)) {
        if (line.rfind("package:", 0) != 0) continue;
        std::istringstream s(line.substr(8)); Package p; p.system = system; s >> p.name;
        std::string uid; s >> uid; if (uid.rfind("uid:", 0) == 0) { try { p.uid = std::stoi(uid.substr(4)); } catch (...) {} }
        if (validPackage(p.name)) out.push_back(p);
    }
    return out;
}
std::vector<ProcessInfo> parseProcesses(const std::string& text) {
    std::vector<ProcessInfo> out; std::istringstream lines(text); std::string line;
    while (std::getline(lines, line)) { std::istringstream s(line); ProcessInfo p; if (s >> p.pid >> p.uid >> p.name && p.pid > 0) out.push_back(p); }
    return out;
}
std::optional<LogEntry> parseLog(const std::string& line) {
    LogEntry e; std::istringstream s(line);
    if (!(s >> e.epoch >> e.pid >> e.tid >> e.level) || !std::isfinite(e.epoch) || e.epoch < 0 || e.pid < 0 || e.tid < 0 || std::string("VDIWEF").find(e.level) == std::string::npos) return {};
    std::string rest; std::getline(s, rest); auto colon = rest.find(':');
    if (colon == std::string::npos) return {};
    e.tag = trim(rest.substr(0, colon)); e.message = rest.substr(colon + 1);
    if (!e.message.empty() && e.message.front() == ' ') e.message.erase(0, 1);
    if (!e.message.empty() && e.message.back() == '\r') e.message.pop_back();
    e.raw=line;if(!e.raw.empty()&&e.raw.back()=='\r')e.raw.pop_back();
    e.lifecycle = e.tag.rfind("am_", 0) == 0 || e.tag.rfind("wm_", 0) == 0;
    return e;
}
static std::vector<std::string> eventFields(const std::string& text) {
    auto a = text.find('['), b = text.rfind(']'); std::vector<std::string> out;
    if (a == std::string::npos || b <= a) return out;
    std::istringstream s(text.substr(a + 1, b - a - 1)); std::string f;
    while (std::getline(s, f, ',')) out.push_back(trim(f)); return out;
}
LogStore::LogStore(size_t capacity, size_t bytes) : capacity_(std::max(size_t(1), capacity)), byteLimit_(std::max(size_t(1024), bytes)) {}
void LogStore::setLimits(size_t capacity,size_t bytes) {
    capacity_=std::max(size_t(1),capacity);byteLimit_=std::max(size_t(1024),bytes);
    limit();++revision_;
}
std::string LogStore::packageFor(const std::string& process) const {
    auto name = process.substr(0, process.find(':'));
    return packages_.count(name) ? name : "";
}
void LogStore::setPackages(const std::vector<Package>& packages) {
    packages_.clear(); for (const auto& p : packages) packages_.insert(p.name);
    bytes_=0;for (auto& e : entries_) { attribute(e);bytes_+=cost(e); } ++revision_;limit();
}
void LogStore::start(int pid, const std::string& name, double epoch) {
    auto& lives = lives_[pid];
    // A snapshot is an observation, not proof that a process existed earlier.
    for (auto& l : lives) if (l.start <= epoch && epoch < l.end) {
        if (l.process == name) return;
        l.end = epoch;
    }
    double endTime = std::numeric_limits<double>::infinity();
    for (const auto& l : lives) if (l.start > epoch) endTime = std::min(endTime, l.start);
    lives.push_back({epoch, endTime, name});
    std::sort(lives.begin(), lives.end(), [](const Lifetime& a, const Lifetime& b) { return a.start < b.start; });
    reattribute(pid);
}
void LogStore::end(int pid, double epoch) {
    auto found = lives_.find(pid); if (found == lives_.end()) return;
    bool changed=false;
    for (auto& l : found->second) if (l.start <= epoch && epoch < l.end) { l.end = epoch;changed=true; }
    if(changed)reattribute(pid);
}
void LogStore::snapshot(const std::vector<ProcessInfo>& processes, double epoch) {
    std::set<int> alive;
    for (const auto& p : processes) { alive.insert(p.pid); start(p.pid, p.name, epoch); }
    for (auto& [pid, lives] : lives_) if (!alive.count(pid)) end(pid, epoch);
    processes_ = processes; ++revision_;limit();
}
void LogStore::attribute(LogEntry& e) const {
    e.process.clear(); e.package.clear(); auto found = lives_.find(e.pid);
    if (found != lives_.end()) for (const auto& l : found->second)
        if (l.start <= e.epoch && e.epoch < l.end) { e.process = l.process; e.package = packageFor(l.process); break; }
    e.search = lower(e.tag + " " + e.message + " " + e.process + " " + e.package);
}
void LogStore::reattribute(int pid) { for (auto& e : entries_) if (e.pid == pid) {auto before=cost(e);attribute(e);bytes_=bytes_-before+cost(e);} ++revision_; }
bool LogStore::ingest(const std::string& line) {
    auto parsed = parseLog(line); if (!parsed) return false; auto e = std::move(*parsed);
    auto f = eventFields(e.message);
    try {
        if (e.tag == "am_proc_start" && f.size() >= 4) start(std::stoi(f[1]), f[3], e.epoch);
        if (e.tag == "am_proc_died" && f.size() >= 3) end(std::stoi(f[1]), e.epoch);
    } catch (...) {}
    e.id = nextId_++; attribute(e);
    bytes_ += e.tag.size() + e.message.size() + sizeof(LogEntry) + e.search.size() + e.process.size() + e.package.size() + e.raw.size();
    entries_.push_back(std::move(e)); ++revision_; limit(); return true;
}
static size_t cost(const LogEntry& e) { return sizeof(LogEntry) + e.tag.size() + e.message.size() + e.search.size() + e.process.size() + e.package.size() + e.raw.size(); }
void LogStore::limit() {
    while (!entries_.empty() && (entries_.size() > capacity_ || bytes_ > byteLimit_)) {
        const auto n = cost(entries_.front()); bytes_ = n > bytes_ ? 0 : bytes_ - n; entries_.pop_front(); ++discarded_;
    }
    if (discarded_ && discarded_ % 512 == 0 && !entries_.empty()) {
        double oldest = entries_.front().epoch;
        for (auto it = lives_.begin(); it != lives_.end();) {
            auto& v = it->second; v.erase(std::remove_if(v.begin(), v.end(), [oldest](const Lifetime& l) { return l.end < oldest; }), v.end());
            if (v.empty()) it = lives_.erase(it); else ++it;
        }
    }
}
void LogStore::clear() { entries_.clear(); bytes_ = 0; discarded_ = 0; ++revision_; }
void LogStore::reset() { clear(); lives_.clear(); packages_.clear(); processes_.clear(); }
bool LogFilter::matches(const LogEntry& e) const {
    if (severity(e.level) < minimum || (pid > 0 && e.pid != pid) || (hideLifecycle && e.lifecycle)) return false;
    if (onlyUnassigned && !e.package.empty()) return false;
    if (!packages.empty() && !packages.count(e.package)) return false;
    if (!tag.empty() && lower(e.tag).find(tag) == std::string::npos) return false;
    return (text.empty() || e.search.find(text) != std::string::npos) && (exclude.empty() || e.search.find(exclude) == std::string::npos);
}
std::string formatLog(const LogEntry& e) {
    std::ostringstream out; out << std::fixed << std::setprecision(6) << e.epoch << " " << e.level << " " << e.pid << "/" << e.tid << " [" << (e.package.empty() ? "system/unknown" : e.package) << "] " << e.tag << ": " << e.message; return out.str();
}
std::string rawLog(const LogEntry& e) { return e.raw.empty()?formatLog(e):e.raw; }
bool logLess(const LogEntry& a,const LogEntry& b,const std::vector<LogSort>& order) {
    for(const auto& sort:order){int result=0;
        auto compare=[&](const auto& x,const auto& y){return x<y?-1:y<x?1:0;};
        switch(sort.column){
        case LogColumn::time:result=compare(a.epoch,b.epoch);break;
        case LogColumn::level:result=compare(severity(a.level),severity(b.level));break;
        case LogColumn::pid:result=compare(a.pid,b.pid);if(!result)result=compare(a.tid,b.tid);break;
        case LogColumn::application:result=a.package.compare(b.package);break;
        case LogColumn::tag:result=a.tag.compare(b.tag);break;
        case LogColumn::message:result=a.message.compare(b.message);break;
        }
        if(result)return sort.descending?result>0:result<0;
    }
    return a.id<b.id;
}
bool logFollow(bool following,float previous,float current,float maximum,bool scrollUp,float tolerance) {
    if(scrollUp)return false;
    const bool bottom=current>=maximum-tolerance;
    if(previous>=0&&current<previous-tolerance&&!bottom)return false;
    if(previous>=0&&current>previous+tolerance&&bottom)return true;
    return following;
}
static std::string json(const std::string& s) {
    std::ostringstream out; out << '"';
    for (unsigned char c : s) { switch (c) { case '"': out << "\\\""; break; case '\\': out << "\\\\"; break; case '\n': out << "\\n"; break; case '\r': out << "\\r"; break; case '\t': out << "\\t"; break; default: if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec; else out << char(c); } }
    out << '"'; return out.str();
}
std::string exportLogs(const std::vector<const LogEntry*>& entries, bool jsonl) {
    std::ostringstream out;
    for (const auto* e : entries) {
        if (!jsonl) out << formatLog(*e) << '\n';
        else out << "{\"epoch\":" << std::fixed << std::setprecision(6) << e->epoch << ",\"pid\":" << e->pid << ",\"tid\":" << e->tid << ",\"level\":" << json(std::string(1,e->level)) << ",\"package\":" << json(e->package) << ",\"process\":" << json(e->process) << ",\"tag\":" << json(e->tag) << ",\"message\":" << json(e->message) << "}\n";
    } return out.str();
}
}
