// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "quest/documentation.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        auto expect = [](const std::string& actual, const std::string& expected) { if (actual != expected) throw std::runtime_error("Incorrect documentation URI: " + actual + " (expected " + expected + ")"); };
        const std::filesystem::path manual = L"C:\\Example App\\Équipe #1\\Documentation\\Documentation.html";
        const auto base = "file:///C:/Example%20App/%C3%89quipe%20%231/Documentation/Documentation.html";
        expect(quest::documentationUrl(manual,"fr",0), std::string(base)+"#/fr/console");
        expect(quest::documentationUrl(manual,"en",4), std::string(base)+"#/en/device/adb");
        expect(quest::documentationUrl(manual,"xx",99), std::string(base)+"#/en/overview");
        expect(quest::documentationUrl(L"\\\\server\\share name\\Documentation.html","fr",8), "file://server/share%20name/Documentation.html#/fr/metrics");
        expect(quest::documentationUrl(L"C:\\App%Name\\Documentation.html","en",7), "file:///C:/App%25Name/Documentation.html#/en/files");
        std::cout << "Documentation URI: Unicode, spaces, fragments, percent, UNC and contextual routes OK\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
