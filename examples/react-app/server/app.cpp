#include "app.hpp"

#include <inertia/inertia.hpp>
#include <inertia/ports/file_reader.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/ssr.hpp>
#include <inertia/vite.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace example {

namespace {

// Reads the Vite manifest and the dev-server hot file from disk. The stamp (size and modification
// time) lets Vite keep the parsed manifest until `bun run build` rewrites it.
class DiskFileReader final : public inertia::FileReader {
  public:
    [[nodiscard]] std::optional<inertia::FileStamp>
    stamp(const std::filesystem::path &path) const override {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error) {
            return std::nullopt;
        }
        const auto modified = std::filesystem::last_write_time(path, error);
        if (error) {
            return std::nullopt;
        }
        return inertia::FileStamp{.size = size, .modified = modified.time_since_epoch().count()};
    }

    [[nodiscard]] std::optional<std::string>
    read(const std::filesystem::path &path) const override {
        std::ifstream in{path, std::ios::binary};
        if (!in) {
            return std::nullopt;
        }
        std::ostringstream content;
        content << in.rdbuf();
        return content.str();
    }
};

} // namespace

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
                                              std::make_shared<DiskFileReader>());

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
