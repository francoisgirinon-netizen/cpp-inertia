#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/ssr.hpp>
#include <inertia/vite.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace inertia;
using namespace inertia::fake;

namespace {

constexpr const char *kSsrReply =
    R"({"head":["<title>a</title>","<meta x>"],"body":"<div id=\"app\" data-server-rendered=\"true\">x</div>"})";

HttpResult reply(int status, std::string body, std::optional<std::string> error = std::nullopt) {
    return HttpResult{.status = status, .body = std::move(body), .error = std::move(error)};
}

SsrConfig enabledConfig() {
    SsrConfig cfg;
    cfg.enabled = true;
    return cfg;
}

struct GatewayRig {
    std::shared_ptr<FakeHttpClient> http = std::make_shared<FakeHttpClient>();
    std::shared_ptr<MemoryFileReader> files = std::make_shared<MemoryFileReader>();
    SsrConfig ssr = enabledConfig();

    [[nodiscard]] SsrGateway gateway() const {
        return {ssr, http, std::make_shared<const Vite>(ViteConfig{}, files)};
    }

    [[nodiscard]] std::optional<SsrResult> render(const std::string &path = "/home") const {
        std::optional<SsrResult> got;
        bool called = false;
        gateway().render(Json{{"component", "X"}}, path, [&](std::optional<SsrResult> r) {
            got = std::move(r);
            called = true;
        });
        REQUIRE(called);
        return got;
    }
};

} // namespace

TEST_CASE("gateway posts the page JSON and joins the head", "[ssr][protocol:ssr.render]") {
    GatewayRig rig;
    rig.http->replies.push_back(reply(200, kSsrReply));
    const auto got = rig.render();
    REQUIRE(rig.http->posts.size() == 1);
    REQUIRE(rig.http->posts[0].first == "http://127.0.0.1:13714/render");
    REQUIRE(rig.http->posts[0].second == R"({"component":"X"})");
    REQUIRE(got.has_value());
    const SsrResult result = got.value_or(SsrResult{});
    REQUIRE(result.head == "<title>a</title>\n<meta x>");
    REQUIRE(result.body == R"(<div id="app" data-server-rendered="true">x</div>)");
}

TEST_CASE("gateway posts to the Vite dev server while the hot file exists",
          "[ssr][protocol:ssr.render]") {
    GatewayRig rig;
    rig.files->files["public/hot"] = "http://localhost:5173/\n";
    rig.http->replies.push_back(reply(200, kSsrReply));
    REQUIRE(rig.render().has_value());
    REQUIRE(rig.http->posts[0].first == "http://localhost:5173/__inertia_ssr");
}

TEST_CASE("gateway url with a trailing slash has no double slash", "[ssr][protocol:ssr.render]") {
    GatewayRig rig;
    rig.ssr.url = "http://ssr:13714/";
    rig.http->replies.push_back(reply(200, kSsrReply));
    REQUIRE(rig.render().has_value());
    REQUIRE(rig.http->posts[0].first == "http://ssr:13714/render");
}

TEST_CASE("gateway failures yield nullopt", "[ssr][protocol:ssr.fallback]") {
    GatewayRig rig;
    SECTION("connection error") { rig.http->replies.push_back(reply(0, "", "refused")); }
    SECTION("status 500") { rig.http->replies.push_back(reply(500, kSsrReply)); }
    SECTION("invalid json") { rig.http->replies.push_back(reply(200, "not json")); }
    SECTION("not an object") { rig.http->replies.push_back(reply(200, "[1]")); }
    SECTION("head is not an array") {
        rig.http->replies.push_back(reply(200, R"({"head":"x","body":"y"})"));
    }
    SECTION("head holds a non-string") {
        rig.http->replies.push_back(reply(200, R"({"head":[1],"body":"y"})"));
    }
    SECTION("body missing") { rig.http->replies.push_back(reply(200, R"({"head":[]})")); }
    SECTION("no reply queued") {}
    REQUIRE_FALSE(rig.render().has_value());
}

TEST_CASE("shouldRender honours enabled, client and exceptPaths", "[ssr]") {
    GatewayRig rig;
    rig.ssr.exceptPaths = {"/admin*", "/exact"};
    REQUIRE(rig.gateway().shouldRender("/home"));
    REQUIRE_FALSE(rig.gateway().shouldRender("/admin"));
    REQUIRE_FALSE(rig.gateway().shouldRender("/admin/x"));
    REQUIRE_FALSE(rig.gateway().shouldRender("/exact"));
    REQUIRE(rig.gateway().shouldRender("/exactly"));
    rig.ssr.enabled = false;
    REQUIRE_FALSE(rig.gateway().shouldRender("/home"));
    rig.ssr.enabled = true;
    rig.http.reset();
    REQUIRE_FALSE(rig.gateway().shouldRender("/home"));
}

TEST_CASE("health is true only for status OK", "[ssr][protocol:ssr.health]") {
    GatewayRig rig;
    const auto check = [&](HttpResult reply) {
        rig.http->replies.push_back(std::move(reply));
        bool ok = false;
        rig.gateway().health([&](bool v) { ok = v; });
        return ok;
    };
    REQUIRE(check(reply(200, R"({"status":"OK"})")));
    REQUIRE(rig.http->gets.back() == "http://127.0.0.1:13714/health");
    REQUIRE_FALSE(check(reply(200, R"({"status":"DOWN"})")));
    REQUIRE_FALSE(check(reply(500, R"({"status":"OK"})")));
    REQUIRE_FALSE(check(reply(200, "nope")));
    REQUIRE_FALSE(check(reply(0, "", "refused")));
}
