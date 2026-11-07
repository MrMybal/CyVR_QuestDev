// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/documentation.hpp"
#include "cydoc/cydoc_link.hpp"

namespace quest {
std::string documentationUrl(const std::filesystem::path& manual, const std::string& locale, int page) {
    const auto path = std::filesystem::absolute(manual).lexically_normal().generic_u8string();
    std::string uri = path.rfind("//", 0) == 0 ? "file:" : "file:///";
    constexpr char hex[] = "0123456789ABCDEF";
    for (const unsigned char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':') {
            uri += static_cast<char>(c);
        } else {
            uri += '%'; uri += hex[c >> 4]; uri += hex[c & 15];
        }
    }
    static constexpr const char* pages[] = {"console", "applications", "settings", "device", "device", "plugins", "assistant", "files", "metrics"};
    const std::string target = page >= 0 && page < 9 ? pages[page] : "overview";
    return cydoc_native::link(uri, locale == "fr" ? "fr" : "en", target, page == 4 ? "adb" : "");
}
}
