// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <filesystem>
#include <string>

namespace quest {
// Absolute UTF-8 file URI with the reader's stable language/page/section route.
std::string documentationUrl(const std::filesystem::path& manual, const std::string& locale, int page);
}
