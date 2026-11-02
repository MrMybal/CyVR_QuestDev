// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "quest/metrics_data.hpp"
#include <cyui/modules/Plots.h>
namespace quest {
struct MetricPresentation {MetricsPlot statistics;std::vector<cyui::Point> points;};
MetricPresentation prepareMetricPlot(const MetricsCsv&,size_t column,double start,double end,size_t pixels);
}
