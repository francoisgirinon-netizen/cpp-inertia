// Request hot path benchmarks: page(), render() for a first visit (HTML), for an Inertia JSON
// visit, for a partial reload, and the pieces they are made of (script escaping, the Vite
// manifest). Build the `release` preset with -DINERTIA_BUILD_BENCHMARKS=ON and run
// `build/release/benchmarks/inertia_benchmarks`. Catch2 reports the mean per call.
//
// The props are copied before the timed region (they are consumed by render()), so the figures
// are the library's cost, not the handler's cost of building its props. page() takes the props by
// const reference and copies them itself: that copy is part of its figure.

#include "fixtures.hpp"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <inertia/disk_file_reader.hpp>
#include <inertia/fake/fake.hpp>
#include <inertia/html.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>
#include <inertia/vite.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace inertia;

namespace {

constexpr auto kComponent = "Users/Index";

// Runs `call` once per measured iteration, each with its own pre-copied Props.
template <typename Call>
void measureWithProps(Catch::Benchmark::Chronometer &meter, const Props &base, Call call) {
    std::vector<Props> copies(static_cast<std::size_t>(meter.runs()), base);
    meter.measure([&](int run) { return call(std::move(copies[static_cast<std::size_t>(run)])); });
}

// An Inertia instance configured like an application: shared props, and a Vite integration over
// an in-memory manifest, whose hash is the asset version.
struct App {
    std::filesystem::path publicDir =
        std::filesystem::temp_directory_path() /
        ("inertia-bench-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::shared_ptr<const Vite> vite;
    std::unique_ptr<Inertia> inertia;
    fake::MemorySession session;

    App() {
        // The manifest is on disk and read through DiskFileReader, like in an application.
        std::filesystem::create_directories(publicDir / "build" / ".vite");
        std::ofstream{publicDir / "build" / ".vite" / "manifest.json"} << bench::manifestText();
        vite = std::make_shared<const Vite>(ViteConfig{.publicDir = publicDir},
                                            std::make_shared<DiskFileReader>());
        Config config;
        config.vite = vite;
        config.version = versionFromVite(vite);
        inertia = std::make_unique<Inertia>(std::move(config));
        bench::shareTypical(*inertia);
    }
    App(const App &) = delete;
    App &operator=(const App &) = delete;
    App(App &&) = delete;
    App &operator=(App &&) = delete;
    ~App() {
        std::error_code ignored;
        std::filesystem::remove_all(publicDir, ignored);
    }

    // Renders and returns the size of the response body.
    std::size_t render(const fake::FakeRequest &req, Props props) {
        std::size_t size = 0;
        inertia->render(req, session, kComponent, std::move(props),
                        [&size](const Response &res) { size = res.body.size(); });
        return size;
    }
};

} // namespace

TEST_CASE("request hot path", "[.bench]") {
    App app;
    const Props props = bench::makeProps();
    const auto html = fake::FakeRequest::get("/users?page=1");
    const auto xhr = fake::FakeRequest::inertia("/users?page=1", app.inertia->version());
    auto partial = fake::FakeRequest::inertia("/users?page=1", app.inertia->version());
    partial.with("X-Inertia-Partial-Component", kComponent)
        .with("X-Inertia-Partial-Data", "users,stats,opt");

    BENCHMARK("page() full visit") {
        return app.inertia->page(xhr, app.session, kComponent, props);
    };
    BENCHMARK("page() partial reload") {
        return app.inertia->page(partial, app.session, kComponent, props);
    };
    BENCHMARK_ADVANCED("render() first visit (HTML)")(Catch::Benchmark::Chronometer meter) {
        measureWithProps(meter, props, [&](Props p) { return app.render(html, std::move(p)); });
    };
    BENCHMARK_ADVANCED("render() Inertia JSON visit")(Catch::Benchmark::Chronometer meter) {
        measureWithProps(meter, props, [&](Props p) { return app.render(xhr, std::move(p)); });
    };
    BENCHMARK_ADVANCED("render() partial reload")(Catch::Benchmark::Chronometer meter) {
        measureWithProps(meter, props, [&](Props p) { return app.render(partial, std::move(p)); });
    };
    BENCHMARK("handleVersion() matching version") { return app.inertia->handleVersion(xhr); };
}

TEST_CASE("request hot path, large page", "[.bench]") {
    App app;
    const Props props = bench::makeProps(1500);
    const auto html = fake::FakeRequest::get("/users?page=1");
    const auto xhr = fake::FakeRequest::inertia("/users?page=1", app.inertia->version());

    BENCHMARK_ADVANCED("render() first visit (HTML), 296 KB")(Catch::Benchmark::Chronometer meter) {
        measureWithProps(meter, props, [&](Props p) { return app.render(html, std::move(p)); });
    };
    BENCHMARK_ADVANCED("render() Inertia JSON visit, 296 KB")(Catch::Benchmark::Chronometer meter) {
        measureWithProps(meter, props, [&](Props p) { return app.render(xhr, std::move(p)); });
    };
}

TEST_CASE("script escaping", "[.bench]") {
    App app;
    const auto xhr = fake::FakeRequest::inertia("/users?page=1", app.inertia->version());
    const Json typical = app.inertia->page(xhr, app.session, kComponent, bench::makeProps());
    const Json large = app.inertia->page(xhr, app.session, kComponent, bench::makeProps(1500));

    BENCHMARK("dumpJson() typical") { return dumpJson(typical); };
    BENCHMARK("escapeForScript() typical") { return escapeForScript(typical); };
    BENCHMARK("inertiaBody() typical") { return inertiaBody(typical, "app"); };
    BENCHMARK("escapeForScript() large") { return escapeForScript(large); };
}

TEST_CASE("vite manifest", "[.bench]") {
    App app;
    BENCHMARK("Vite::manifestHash()") { return app.vite->manifestHash(); };
    BENCHMARK("Vite::tags()") { return app.vite->tags(); };

    // The in-memory test double stamps by hashing the content, so it costs more than a stat.
    auto files = std::make_shared<fake::MemoryFileReader>();
    files->files["public/build/.vite/manifest.json"] = bench::manifestText();
    const Vite memory(ViteConfig{}, files);
    BENCHMARK("Vite::manifestHash(), MemoryFileReader") { return memory.manifestHash(); };
}
