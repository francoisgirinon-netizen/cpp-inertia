#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/html.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

using namespace inertia;
using namespace inertia::fake;

namespace {

Response renderOne(const Inertia &app, const FakeRequest &req, Props props) {
    MemorySession session;
    Response out;
    bool called = false;
    app.render(req, session, "Users/Index", std::move(props), [&](Response r) {
        out = std::move(r);
        called = true;
    });
    REQUIRE(called);
    return out;
}

} // namespace

TEST_CASE("page object has required keys", "[page][protocol:page.required]") {
    const Inertia app{Config{}};
    MemorySession session;
    const auto page = app.page(FakeRequest::inertia("/users"), session, "Users/Index", {{"a", 1}});
    REQUIRE(
        dumpJson(page) ==
        R"({"component":"Users/Index","props":{"a":1,"errors":{}},"url":"/users","version":""})");
}

TEST_CASE("page url preserves trailing slash and encoding",
          "[page][protocol:page.url.trailing-slash]") {
    const Inertia app{Config{}};
    MemorySession session;
    REQUIRE(app.page(FakeRequest::inertia("/users/"), session, "C", {})["url"] == "/users/");
    REQUIRE(app.page(FakeRequest::inertia("/users/?a=%20b"), session, "C", {})["url"] ==
            "/users/?a=%20b");
}

TEST_CASE("version defaults to empty", "[page][protocol:page.version.empty]") {
    REQUIRE(Inertia{Config{}}.version().empty());
    Config config;
    config.version = [] { return std::string("abc"); };
    const Inertia app{config};
    REQUIRE(app.version() == "abc");
    MemorySession session;
    REQUIRE(app.page(FakeRequest::inertia("/"), session, "C", {})["version"] == "abc");
}

TEST_CASE("Inertia request gets JSON", "[page][protocol:json.response]") {
    const Inertia app{Config{}};
    const auto req = FakeRequest::inertia("/users");
    const auto res = renderOne(app, req, {{"a", 1}});
    REQUIRE(res.status == 200);
    REQUIRE(res.header("X-Inertia") == "true");
    REQUIRE(res.header("Content-Type").value_or("").find("application/json") == 0);
    MemorySession session;
    REQUIRE(Json::parse(res.body) == app.page(req, session, "Users/Index", {{"a", 1}}));
}

TEST_CASE("first visit gets HTML with script element", "[html][protocol:html.script-element]") {
    const Inertia app{Config{}};
    const auto res = renderOne(app, FakeRequest::get("/users"), {{"a", 1}});
    REQUIRE(res.status == 200);
    REQUIRE(res.header("Content-Type") == "text/html; charset=utf-8");
    REQUIRE(res.body.find(R"(<script data-page="app" type="application/json">)") !=
            std::string::npos);
    REQUIRE(res.body.find(R"(</script><div id="app"></div>)") != std::string::npos);
}

TEST_CASE("root id is configurable", "[html][protocol:html.root-id]") {
    Config config;
    config.rootId = "root";
    const Inertia app{config};
    const auto res = renderOne(app, FakeRequest::get("/"), {});
    REQUIRE(res.body.find(R"(data-page="root")") != std::string::npos);
    REQUIRE(res.body.find(R"(<div id="root">)") != std::string::npos);
}

TEST_CASE("props cannot break out of the script element", "[html][protocol:html.json-escaping]") {
    const Inertia app{Config{}};
    const auto res = renderOne(app, FakeRequest::get("/"),
                               {{"s", "</script><!--x-->"},
                                {"t", "a\xE2\x80\xA8"
                                      "b"}});
    REQUIRE(res.body.find("\\u003C\\/script\\u003E\\u003C!--x--\\u003E") != std::string::npos);
    std::size_t count = 0;
    for (auto pos = res.body.find("</script>"); pos != std::string::npos;
         pos = res.body.find("</script>", pos + 1)) {
        ++count;
    }
    REQUIRE(count == 1);
    REQUIRE(res.body.find("<!--") == std::string::npos);
    REQUIRE(res.body.find("\xE2\x80\xA8") == std::string::npos);
}

TEST_CASE("invalid UTF-8 prop does not throw", "[page]") {
    const Inertia app{Config{}};
    Response res;
    REQUIRE_NOTHROW(res = renderOne(app, FakeRequest::get("/"), {{"s", std::string("\xff")}}));
    REQUIRE(res.status == 200);
    REQUIRE(res.body.find("\xEF\xBF\xBD") != std::string::npos);
    REQUIRE_NOTHROW(res = renderOne(app, FakeRequest::inertia("/"), {{"s", std::string("\xff")}}));
    REQUIRE(res.status == 200);
}

TEST_CASE("invalid UTF-8 prop on the JSON path is replaced by U+FFFD", "[page]") {
    const Inertia app{Config{}};
    const auto res = renderOne(app, FakeRequest::inertia("/"), {{"s", std::string("\xff")}});
    REQUIRE(res.status == 200);
    REQUIRE(res.body.find("\xEF\xBF\xBD") != std::string::npos);
}

TEST_CASE("an HTML response does not carry X-Inertia", "[html][protocol:json.response]") {
    const Inertia app{Config{}};
    const auto res = renderOne(app, FakeRequest::get("/users"), {{"a", 1}});
    REQUIRE_FALSE(res.header("X-Inertia").has_value());
}

TEST_CASE("finalize adds Vary: X-Inertia", "[page][protocol:vary.always]") {
    const Inertia app{Config{}};
    const auto req = FakeRequest::get("/");
    const auto vary = [&](std::optional<std::string> initial) {
        Response res;
        if (initial) {
            res.setHeader("Vary", *initial);
        }
        app.finalize(req, res);
        return res.header("Vary").value_or("");
    };
    REQUIRE(vary(std::nullopt) == "X-Inertia");
    REQUIRE(vary("Accept-Encoding") == "Accept-Encoding, X-Inertia");
    REQUIRE(vary("X-Inertia") == "X-Inertia");
    REQUIRE(vary("x-inertia") == "x-inertia");
    REQUIRE(vary(", Accept") == "Accept, X-Inertia");
}
