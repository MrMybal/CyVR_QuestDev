#pragma once
#include "quest/backend.hpp"
#include <memory>

namespace quest {
struct Options { bool demo{},compare{}; int page{},pluginTab{}; std::wstring adb; std::string package,plugin;void* graphicsDevice{}; };
class App {
public:
    explicit App(Options options);
    ~App();
    void frame();
    std::string diagnostics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
