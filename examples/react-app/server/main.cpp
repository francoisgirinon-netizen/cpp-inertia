#include "routes.hpp"

#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>
#include <inertia/vite.hpp>

#include <drogon/HttpAppFramework.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view kPublicDir = INERTIA_EXAMPLE_PUBLIC_DIR;
constexpr unsigned short kPort = 8080;
constexpr int kThreads = 4; // some demo resolvers sleep, so keep a few workers

// Reads the Vite manifest and the dev-server hot file from disk.
class DiskFileReader final : public inertia::FileReader {
  public:
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

bool ssrEnabled() {
    const char *flag = std::getenv("INERTIA_SSR"); // NOLINT(concurrency-mt-unsafe): startup only
    return flag != nullptr && std::string_view{flag} == "1";
}

} // namespace

int main() {
    auto &app = ::drogon::app();
    app.enableSession(std::chrono::hours(1));
    app.setThreadNum(kThreads);
    app.setDocumentRoot(std::string{kPublicDir}); // serves /build/assets/*

    // Production tags come from public/build/.vite/manifest.json; while public/hot exists
    // (`bun run dev`), they point at the Vite dev server instead.
    auto vite = std::make_shared<const inertia::Vite>(
        inertia::ViteConfig{.publicDir = std::string{kPublicDir},
                            .buildDir = "build",
                            .hotFile = "hot",
                            .entries = {"src/app.tsx"}},
        std::make_shared<DiskFileReader>());

    inertia::Config config;
    config.version = inertia::versionFromVite(vite);
    config.vite = std::move(vite);
    config.ssr.enabled = ssrEnabled(); // INERTIA_SSR=1, with `bun run ssr` listening on :13714

    auto inertiaApp = std::make_shared<inertia::Inertia>(
        std::move(config), std::make_shared<inertia::drogon::DrogonHttpClient>());
    inertiaApp->share("appName", "cpp-inertia demo");

    example::registerHome();
    example::registerUsers();
    example::registerDashboard();
    example::registerFeed();
    example::registerPosts();

    // Must come last: Inertia is immutable once installed.
    inertia::drogon::install(inertiaApp);

    app.addListener("0.0.0.0", kPort);
    app.run();
    return 0;
}
