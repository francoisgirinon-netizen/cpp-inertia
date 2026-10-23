#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/vite.hpp>

#include <memory>
#include <string>
#include <string_view>

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

namespace {

constexpr std::string_view kManifestPath = "public/build/.vite/manifest.json";

constexpr std::string_view kManifest = R"({
  "src/app.tsx": {"file": "assets/app-1.js", "css": ["assets/app-1.css"], "imports": ["_vendor.js"]},
  "_vendor.js": {"file": "assets/vendor-2.js", "css": ["assets/v.css"]}
})";

std::size_t countOf(const std::string &haystack, const std::string &needle) {
    std::size_t count = 0;
    for (auto pos = haystack.find(needle); pos != std::string::npos;
         pos = haystack.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

} // namespace

TEST_CASE("prod tags come from the manifest with css, preloads and entry script", "[vite]") {
    auto files = makeFiles();
    files->files[std::string(kManifestPath)] = std::string(kManifest);
    const Vite vite(ViteConfig{}, files);
    const std::string expected =
        R"V(<link rel="stylesheet" href="/build/assets/app-1.css">)V"
        R"V(<link rel="stylesheet" href="/build/assets/v.css">)V"
        R"V(<link rel="modulepreload" href="/build/assets/vendor-2.js">)V"
        R"V(<script type="module" src="/build/assets/app-1.js"></script>)V";
    REQUIRE(vite.tags() == expected);
}

TEST_CASE("prod tags visit shared imports once", "[vite]") {
    auto files = makeFiles();
    files->files[std::string(kManifestPath)] =
        R"({"src/app.tsx": {"file": "a.js", "imports": ["_v.js"]},
            "src/admin.tsx": {"file": "b.js", "imports": ["_v.js"]},
            "_v.js": {"file": "v.js"}})";
    ViteConfig cfg;
    cfg.entries = {"src/app.tsx", "src/admin.tsx"};
    const Vite vite(cfg, files);
    const auto tags = vite.tags();
    REQUIRE(countOf(tags, R"(href="/build/v.js")") == 1);
    REQUIRE(countOf(tags, R"(src="/build/a.js")") == 1);
    REQUIRE(countOf(tags, R"(src="/build/b.js")") == 1);
}

TEST_CASE("an entry missing from the manifest is skipped", "[vite]") {
    auto files = makeFiles();
    files->files[std::string(kManifestPath)] = std::string(kManifest);
    ViteConfig cfg;
    cfg.entries = {"src/unknown.tsx"};
    const Vite vite(cfg, files);
    REQUIRE(vite.tags().empty());
}

TEST_CASE("malformed manifest JSON is treated as missing and never throws", "[vite]") {
    auto files = makeFiles();
    files->files[std::string(kManifestPath)] = R"({"src/app.tsx": {"file":)";
    const Vite vite(ViteConfig{}, files);
    std::string tags;
    REQUIRE_NOTHROW(tags = vite.tags());
    REQUIRE(tags.empty());
}

TEST_CASE("manifestHash is FNV-1a 64 of the raw manifest, 16 lowercase hex digits",
          "[vite][protocol:asset.version-default]") {
    auto files = makeFiles();
    files->files[std::string(kManifestPath)] = "{}";
    const Vite vite(ViteConfig{}, files);
    REQUIRE(vite.manifestHash() == "08f44b07b5901a25");
}

TEST_CASE("manifestHash is empty without a manifest or with malformed JSON",
          "[vite][protocol:asset.version-default]") {
    auto files = makeFiles();
    REQUIRE(Vite(ViteConfig{}, files).manifestHash().empty());
    files->files[std::string(kManifestPath)] = R"({"src/app.tsx": )";
    REQUIRE_NOTHROW(Vite(ViteConfig{}, files).manifestHash());
    REQUIRE(Vite(ViteConfig{}, files).manifestHash().empty());
}

TEST_CASE("versionFromVite is stable for the same manifest and changes with it",
          "[vite][protocol:asset.version-default]") {
    auto files = makeFiles();
    auto vite = std::make_shared<const Vite>(ViteConfig{}, files);
    const auto version = versionFromVite(vite);
    REQUIRE(version().empty());

    files->files[std::string(kManifestPath)] = kManifest;
    const auto first = version();
    REQUIRE(first.size() == 16);
    REQUIRE(version() == first);
    REQUIRE(std::make_shared<const Vite>(ViteConfig{}, files)->manifestHash() == first);

    files->files[std::string(kManifestPath)] = std::string(kManifest) + " ";
    REQUIRE(version() != first);
}

TEST_CASE("versionFromVite without a Vite is empty", "[vite][protocol:asset.version-default]") {
    REQUIRE(versionFromVite(nullptr)().empty());
}
