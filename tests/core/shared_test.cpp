#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>

#include <string>
#include <vector>

using namespace inertia;
using namespace inertia::fake;

namespace {

Json pageOf(const Inertia &app, const Props &props = {}) {
    MemorySession session;
    return app.page(FakeRequest::inertia("/"), session, "Page", props);
}

std::string keysOf(const Json &page) {
    std::string keys;
    for (const auto &item : page.items()) {
        keys += keys.empty() ? "" : ",";
        keys += item.key();
    }
    return keys;
}

} // namespace

TEST_CASE("shared props list their keys with errors last", "[shared][protocol:page.shared-props]") {
    Inertia app{Config{}};
    app.share("auth", Json::object({{"id", 1}}));
    app.shareWith([](const RequestView &, SessionStore &) -> Props { return {{"locale", "fr"}}; });
    const auto page = pageOf(app, {{"title", "Home"}});
    REQUIRE(dumpJson(page["sharedProps"]) == R"(["auth","locale","errors"])");
    REQUIRE(dumpJson(page["props"]) ==
            R"({"auth":{"id":1},"locale":"fr","title":"Home","errors":{}})");
}

TEST_CASE("a dot key contributes its first segment to sharedProps",
          "[shared][protocol:page.shared-props]") {
    Inertia app{Config{}};
    app.share("flash.x", 1);
    app.share("flash.y", 2);
    app.share("auth", 3);
    const auto page = pageOf(app);
    REQUIRE(dumpJson(page["sharedProps"]) == R"(["flash","auth","errors"])");
    REQUIRE(dumpJson(page["props"]) == R"({"flash":{"x":1,"y":2},"auth":3,"errors":{}})");
}

TEST_CASE("shareOnce registers a shared key", "[shared][protocol:page.shared-props]") {
    Inertia app{Config{}};
    app.shareOnce("plans", [] { return Json::array({"a"}); });
    const auto page = pageOf(app);
    REQUIRE(dumpJson(page["sharedProps"]) == R"(["plans","errors"])");
    REQUIRE(dumpJson(page["props"]) == R"({"plans":["a"],"errors":{}})");
}

TEST_CASE("sharedProps can be turned off", "[shared][protocol:page.shared-props]") {
    Config config;
    config.exposeSharedPropKeys = false;
    Inertia app{config};
    app.share("auth", 1);
    const auto page = pageOf(app);
    REQUIRE_FALSE(page.contains("sharedProps"));
    REQUIRE(dumpJson(page["props"]) == R"({"auth":1,"errors":{}})");
}

TEST_CASE("sharedProps is omitted when nothing is shared", "[shared][protocol:page.shared-props]") {
    REQUIRE_FALSE(pageOf(Inertia{Config{}}, {{"a", 1}}).contains("sharedProps"));
}

TEST_CASE("a page prop replaces a shared prop in place", "[shared]") {
    Inertia app{Config{}};
    app.share("a", 1);
    app.share("b", 2);
    REQUIRE(dumpJson(pageOf(app, {{"a", 9}, {"c", 3}})["props"]) ==
            R"({"a":9,"b":2,"c":3,"errors":{}})");
}

TEST_CASE("a page dot key merges into the shared nested object", "[shared]") {
    Inertia app{Config{}};
    app.share("auth", Json::object({{"user", "u"}}));
    REQUIRE(dumpJson(pageOf(app, {{"auth.token", "t"}})["props"]) ==
            R"({"auth":{"user":"u","token":"t"},"errors":{}})");
}

TEST_CASE("per-request shares see the request and session", "[shared]") {
    Inertia app{Config{}};
    app.shareWith([](const RequestView &req, SessionStore &session) -> Props {
        return {{"url", req.url()}, {"user", session.get("user").value_or(Json())}};
    });
    MemorySession session;
    session.put("user", "ada");
    const auto page = app.page(FakeRequest::inertia("/x"), session, "Page", {});
    REQUIRE(dumpJson(page["props"]) == R"({"url":"/x","user":"ada","errors":{}})");
}

TEST_CASE("metadata keys are omitted when empty", "[shared][protocol:page.metadata.omit-empty]") {
    const Inertia app{Config{}};
    const auto page = pageOf(app, {{"a", 1}, {"b", optional([] { return 2; })}});
    REQUIRE(keysOf(page) == "component,props,url,version");
}

TEST_CASE("a page dot key merges into a shared closure lazily", "[shared]") {
    Inertia app{Config{}};
    int calls = 0;
    app.share("auth", [&calls] {
        ++calls;
        return Json::object({{"user", "u"}});
    });
    REQUIRE(dumpJson(pageOf(app, {{"auth.token", "t"}})["props"]) ==
            R"({"auth":{"user":"u","token":"t"},"errors":{}})");
    REQUIRE(calls == 1);

    MemorySession session;
    auto req = FakeRequest::inertia("/");
    req.with("X-Inertia-Partial-Component", "Page").with("X-Inertia-Partial-Data", "other");
    const auto page = app.page(req, session, "Page", {{"auth.token", "t"}, {"other", 1}});
    REQUIRE(dumpJson(page["props"]) == R"({"other":1,"errors":{}})");
    REQUIRE(calls == 1);
}

TEST_CASE("a dot key merged into a prop keeps its flags", "[shared]") {
    Inertia app{Config{}};
    app.share("auth", always(Json::object({{"user", "u"}})));
    MemorySession session;
    auto req = FakeRequest::inertia("/");
    req.with("X-Inertia-Partial-Component", "Page").with("X-Inertia-Partial-Data", "other");
    const auto page = app.page(req, session, "Page", {{"auth.token", "t"}, {"other", 1}});
    REQUIRE(dumpJson(page["props"]) ==
            R"({"auth":{"user":"u","token":"t"},"other":1,"errors":{}})");
}
