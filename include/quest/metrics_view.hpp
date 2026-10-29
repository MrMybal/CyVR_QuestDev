// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "quest/metrics_capture.hpp"
namespace quest {
class MetricsView {
public:
    MetricsView(std::filesystem::path root,bool demo);
    ~MetricsView();
    MetricsCapture& capture();
    void sidebar();
    void draw();
    nlohmann::json describe()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
