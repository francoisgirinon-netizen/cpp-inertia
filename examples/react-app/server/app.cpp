#include "app.hpp"

#include <inertia/disk_file_reader.hpp>
#include <inertia/inertia.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/ssr.hpp>
#include <inertia/vite.hpp>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace example {

namespace {

// Whether the environment variable `name` is "1". std::getenv is deprecated on MSVC (C4996), which
// has _dupenv_s instead.
bool envFlag(const char *name) {
#if defined(_MSC_VER)
    char *raw = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&raw, &size, name) != 0 || raw == nullptr) {
        return false;
    }
    const std::unique_ptr<char, decltype(&std::free)> owned{raw, &std::free};
    return std::string_view{owned.get()} == "1";
#else
    const char *value = std::getenv(name);
    return value != nullptr && std::string_view{value} == "1";
#endif
}

} // namespace

const std::string &publicDir() {
    static const std::string dir = INERTIA_EXAMPLE_PUBLIC_DIR;
    return dir;
}

std::shared_ptr<inertia::Inertia> makeInertia(std::shared_ptr<inertia::HttpClient> client) {
    const bool ssrEnabled = envFlag("INERTIA_SSR");

    // Production tags come from public/build/.vite/manifest.json; while public/hot exists
    // (`bun run dev`), they point at the Vite dev server instead.
    auto vite =
        std::make_shared<const inertia::Vite>(inertia::ViteConfig{.publicDir = publicDir(),
                                                                  .buildDir = "build",
                                                                  .hotFile = "hot",
                                                                  .entries = {"src/app.tsx"}},
                                              std::make_shared<inertia::DiskFileReader>());

    inertia::Config config;
    // The asset version is the hash of the Vite manifest, recomputed when the manifest file
    // changes. After `bun run build` rewrites it, a client holding the old version sends a stale
    // X-Inertia-Version on its next visit, gets a 409 + X-Inertia-Location and does a full reload.
    config.version = inertia::versionFromVite(vite);
    config.vite = std::move(vite);
    config.ssr.enabled = ssrEnabled;

    auto inertiaApp = std::make_shared<inertia::Inertia>(std::move(config), std::move(client));
    inertiaApp->share("appName", "cpp-inertia demo");
    return inertiaApp;
}

} // namespace example
