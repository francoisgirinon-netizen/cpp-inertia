#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/html.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>
#include <inertia/ssr.hpp>
#include <inertia/vite.hpp>

#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

namespace {

struct RenderRig {
    GatewayRig rig;

    [[nodiscard]] Response get(const std::string &url, bool inertia = false) const {
        Config cfg;
        cfg.ssr = rig.ssr;
        cfg.vite = std::make_shared<const Vite>(ViteConfig{}, rig.files);
        const Inertia app(std::move(cfg), rig.http);
        MemorySession session;
        Response out;
        bool called = false;
        const auto req = inertia ? FakeRequest::inertia(url) : FakeRequest::get(url);
        app.render(req, session, "Home", {}, [&](Response r) {
            out = std::move(r);
            called = true;
        });
        REQUIRE(called);
        return out;
    }
};

} // namespace

TEST_CASE("SSR output replaces the script element and root div", "[ssr][protocol:ssr.render]") {
    RenderRig r;
    r.rig.http->replies.push_back(reply(200, kSsrReply));
    const auto res = r.get("/home");
    REQUIRE(r.rig.http->posts.size() == 1);
    REQUIRE(r.rig.http->posts[0].first == "http://127.0.0.1:13714/render");
    const auto sent = Json::parse(r.rig.http->posts[0].second);
    REQUIRE(sent["component"] == "Home");
    REQUIRE(sent["url"] == "/home");
    REQUIRE(res.status == 200);
    REQUIRE(res.body.find("<head><meta charset=\"utf-8\"><title>a</title>\n<meta x></head>") !=
            std::string::npos);
    REQUIRE(res.body.find(R"(<div id="app" data-server-rendered="true">x</div>)") !=
            std::string::npos);
    REQUIRE(res.body.find("data-page=") == std::string::npos);
}

TEST_CASE("SSR failures serve the client-side page with a 200", "[ssr][protocol:ssr.fallback]") {
    RenderRig r;
    SECTION("connection error") { r.rig.http->replies.push_back(reply(0, "", "refused")); }
    SECTION("status 500") { r.rig.http->replies.push_back(reply(500, kSsrReply)); }
    SECTION("invalid json") { r.rig.http->replies.push_back(reply(200, "not json")); }
    const auto res = r.get("/home");
    REQUIRE(r.rig.http->posts.size() == 1);
    REQUIRE(res.status == 200);
    REQUIRE(res.body.find("data-page=") != std::string::npos);
}

TEST_CASE("excluded paths are not server-rendered", "[ssr]") {
    RenderRig r;
    r.rig.ssr.exceptPaths = {"/admin*"};
    r.rig.http->replies.push_back(reply(200, kSsrReply));
    const auto res = r.get("/admin/x?tab=1");
    REQUIRE(r.rig.http->posts.empty());
    REQUIRE(res.body.find("data-page=") != std::string::npos);
}

TEST_CASE("the query string is ignored when matching excluded paths", "[ssr]") {
    RenderRig r;
    r.rig.ssr.exceptPaths = {"/exact"};
    r.rig.http->replies.push_back(reply(200, kSsrReply));
    (void)r.get("/exact?x=1");
    REQUIRE(r.rig.http->posts.empty());
}

TEST_CASE("disabled SSR never posts", "[ssr]") {
    RenderRig r;
    r.rig.ssr.enabled = false;
    r.rig.http->replies.push_back(reply(200, kSsrReply));
    const auto res = r.get("/home");
    REQUIRE(r.rig.http->posts.empty());
    REQUIRE(res.body.find("data-page=") != std::string::npos);
}

TEST_CASE("Inertia requests never call SSR", "[ssr]") {
    RenderRig r;
    r.rig.http->replies.push_back(reply(200, kSsrReply));
    const auto res = r.get("/home", true);
    REQUIRE(r.rig.http->posts.empty());
    REQUIRE(res.header("Content-Type") == "application/json");
}

namespace {

// Throws from post/get, optionally after having answered the call.
class ThrowingClient final : public HttpClient {
  public:
    bool answerFirst = false;

    void post(std::string /*url*/, std::string /*jsonBody*/, std::chrono::milliseconds /*timeout*/,
              std::function<void(HttpResult)> done) override {
        if (answerFirst) {
            done(reply(200, kSsrReply));
        }
        throw std::runtime_error("client exploded");
    }
    void get(std::string /*url*/, std::chrono::milliseconds /*timeout*/,
             std::function<void(HttpResult)> done) override {
        if (answerFirst) {
            done(reply(200, R"({"status":"OK"})"));
        }
        throw std::runtime_error("client exploded");
    }
};

// Keeps the callback so the test can answer after render() has returned.
class DeferredClient final : public HttpClient {
  public:
    std::function<void(HttpResult)> pending;

    void post(std::string /*url*/, std::string /*jsonBody*/, std::chrono::milliseconds /*timeout*/,
              std::function<void(HttpResult)> done) override {
        pending = std::move(done);
    }
    void get(std::string /*url*/, std::chrono::milliseconds /*timeout*/,
             std::function<void(HttpResult)> done) override {
        pending = std::move(done);
    }
};

} // namespace

TEST_CASE("a throwing client falls back to client-side rendering exactly once",
          "[ssr][protocol:ssr.fallback]") {
    auto client = std::make_shared<ThrowingClient>();
    client->answerFirst = GENERATE(false, true);
    Config cfg;
    cfg.ssr = enabledConfig();
    const Inertia app(std::move(cfg), client);
    MemorySession session;
    int calls = 0;
    Response out;
    app.render(FakeRequest::get("/home"), session, "Home", {}, [&](Response r) {
        ++calls;
        out = std::move(r);
    });
    REQUIRE(calls == 1);
    REQUIRE(out.status == 200);
    if (!client->answerFirst) {
        REQUIRE(out.body.find("data-page=") != std::string::npos);
    }
}

TEST_CASE("a throwing client makes health false exactly once", "[ssr][protocol:ssr.health]") {
    auto client = std::make_shared<ThrowingClient>();
    client->answerFirst = GENERATE(false, true);
    const SsrGateway gw(enabledConfig(), client, nullptr);
    int calls = 0;
    bool last = true;
    gw.health([&](bool v) {
        ++calls;
        last = v;
    });
    REQUIRE(calls == 1);
    REQUIRE(last == client->answerFirst);
}

TEST_CASE("exceptions from the completion callback are not swallowed", "[ssr]") {
    auto client = std::make_shared<ThrowingClient>();
    client->answerFirst = true;
    Config cfg;
    cfg.ssr = enabledConfig();
    const Inertia app(std::move(cfg), client);
    MemorySession session;
    REQUIRE_THROWS_AS(app.render(FakeRequest::get("/home"), session, "Home", {},
                                 [](const Response &) { throw std::logic_error("user"); }),
                      std::logic_error);
}

TEST_CASE("an async SSR answer outlives the Inertia instance", "[ssr]") {
    auto client = std::make_shared<DeferredClient>();
    MemorySession session;
    int calls = 0;
    Response out;
    {
        Config cfg;
        cfg.ssr = enabledConfig();
        cfg.rootId = "root";
        cfg.rootTemplate = [](const RootContext &ctx) {
            return "<html>" + ctx.head + ctx.body + "</html>";
        };
        const Inertia app(std::move(cfg), client);
        app.render(FakeRequest::get("/home"), session, "Home", {}, [&](Response r) {
            ++calls;
            out = std::move(r);
        });
        REQUIRE(calls == 0);
    }
    client->pending(reply(500, ""));
    REQUIRE(calls == 1);
    REQUIRE(out.body.find(R"(data-page="root")") != std::string::npos);
    REQUIRE(out.body.rfind("<html>", 0) == 0);
}
