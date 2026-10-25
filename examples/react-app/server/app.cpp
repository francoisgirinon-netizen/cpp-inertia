#include "app.hpp"

#include <inertia/disk_file_reader.hpp>
#include <inertia/inertia.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/ssr.hpp>
#include <inertia/vite.hpp>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace example {

const std::string &publicDir() {
    static const std::string dir = INERTIA_EXAMPLE_PUBLIC_DIR;
    return dir;
}

std::shared_ptr<inertia::Inertia> makeInertia(std::shared_ptr<inertia::HttpClient> client) {
    const char *flag = std::getenv("INERTIA_SSR");
    const bool ssrEnabled = flag != nullptr && std::string_view{flag} == "1";

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
