// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "quest/backend.hpp"
#include <memory>

namespace quest {
struct Options { bool demo{},compare{},mirrorSetup{},updatesSettings{}; int page{},pluginTab{}; std::wstring adb,metricsCsv,scrcpy; std::string package,plugin;void* graphicsDevice{};void* windowHandle{}; };
class App {
public:
    explicit App(Options options);
    ~App();
    void frame();
    std::string diagnostics() const;
    bool readyForUpdate() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
