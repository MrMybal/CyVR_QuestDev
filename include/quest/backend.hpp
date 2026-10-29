#pragma once
#include "quest/core.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <memory>
#include <thread>

namespace quest {
struct CommandResult { int code{-1}; std::string output; bool timedOut{}; };
std::wstring widen(const std::string& text);
std::string narrow(const std::wstring& text);
std::wstring findAdb();
CommandResult runProcess(const std::wstring& exe, const std::vector<std::string>& args,
    int timeoutMs = 10000, const std::atomic<bool>* cancel = nullptr,
    const std::function<void(const std::string&)>& chunk = {}, const std::wstring& outputFile = {});

struct CommandStep { std::vector<std::string> args; std::wstring outputFile; };
struct JobRequest { std::string label; std::vector<CommandStep> steps; int timeoutMs{30000}; bool needsDevice{true}; std::vector<CommandStep> cleanupSteps; };
std::vector<std::string> parseCommand(const std::string& text, std::string& error);
std::string shellQuote(const std::string& text);

struct BackendEvent {
    enum Kind { Devices, Packages, Processes, Lines, Status, Operation, JobStarted, JobOutput, JobDone } kind{Status};
    uint64_t session{};
    std::vector<Device> devices;
    std::vector<Package> packages;
    std::vector<ProcessInfo> processes;
    std::vector<std::string> lines;
    std::string text;
    double epoch{};
    bool ok{};
    uint64_t job{};
    int code{};
    bool cancelled{};
    std::string label;
};
class Backend {
public:
    explicit Backend(std::wstring adb);
    ~Backend();
    uint64_t select(const std::string& serial);
    void refresh();
    void action(std::string action, std::string value);
    void setAdb(std::wstring path);
    uint64_t submit(JobRequest request);
    void cancel(uint64_t id);
    std::vector<BackendEvent> poll();
    uint64_t dropped() const { return dropped_; }
private:
    struct Request { std::string action, value; uint64_t session{}; };
    std::mutex mutex_, eventMutex_;
    std::condition_variable wake_;
    std::wstring adb_;
    std::string desired_;
    std::deque<Request> requests_;
    std::deque<BackendEvent> events_;
    size_t queuedBytes_{};
    std::atomic<bool> quit_{false}, stopStream_{false};
    std::atomic<bool> streamEnded_{false};
    std::atomic<uint64_t> session_{0}, dropped_{0};
    bool refresh_{true};
    std::thread worker_, stream_;
    struct Job { uint64_t id{},session{}; std::atomic<bool> cancel{false},done{false}; std::thread thread; };
    std::mutex jobsMutex_;
    std::vector<std::shared_ptr<Job>> jobs_;
    std::atomic<uint64_t> nextJob_{1};
    void loop();
    void push(BackendEvent e);
    void stopCapture();
};
}
