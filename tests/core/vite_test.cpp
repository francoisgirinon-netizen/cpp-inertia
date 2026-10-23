#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/vite.hpp>

#include <memory>
#include <string>

using namespace inertia;

namespace {

std::shared_ptr<fake::MemoryFileReader> makeFiles() {
    return std::make_shared<fake::MemoryFileReader>();
}

} // namespace

TEST_CASE("hotUrl trims the hot file content", "[vite]") {
    auto files = makeFiles();
    files->files["public/hot"] = "http://localhost:5173/\n";
    const Vite vite(ViteConfig{}, files);
    REQUIRE(vite.hotUrl() == "http://localhost:5173");
}

TEST_CASE("hotUrl is empty when there is no hot file", "[vite]") {
    const Vite vite(ViteConfig{}, makeFiles());
    REQUIRE_FALSE(vite.hotUrl().has_value());
}

TEST_CASE("dev tags load the refresh preamble, client and entries from the hot URL", "[vite]") {
    auto files = makeFiles();
    files->files["public/hot"] = "http://localhost:5173\n";
    const Vite vite(ViteConfig{}, files);
    const std::string expected =
        R"V(<script type="module">import RefreshRuntime from 'http://localhost:5173/@react-refresh';RefreshRuntime.injectIntoGlobalHook(window);window.$RefreshReg$ = () => {};window.$RefreshSig$ = () => (type) => type;window.__vite_plugin_react_preamble_installed__ = true;</script>)V"
        R"V(<script type="module" src="http://localhost:5173/@vite/client"></script>)V"
        R"V(<script type="module" src="http://localhost:5173/src/app.tsx"></script>)V";
    REQUIRE(vite.tags() == expected);
}

TEST_CASE("dev tags omit the refresh preamble when disabled", "[vite]") {
    auto files = makeFiles();
    files->files["public/hot"] = "http://localhost:5173";
    ViteConfig cfg;
    cfg.reactRefresh = false;
    const Vite vite(cfg, files);
    REQUIRE(vite.tags() ==
            R"V(<script type="module" src="http://localhost:5173/@vite/client"></script>)V"
            R"V(<script type="module" src="http://localhost:5173/src/app.tsx"></script>)V");
}

TEST_CASE("no hot file and no manifest give empty tags without throwing", "[vite]") {
    const Vite vite(ViteConfig{}, makeFiles());
    std::string tags;
    REQUIRE_NOTHROW(tags = vite.tags());
    REQUIRE(tags.empty());
}
