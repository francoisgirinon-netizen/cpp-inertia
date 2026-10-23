#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>
#include <inertia/session.hpp>

#include <cstdint>
#include <string>

using namespace inertia;
using namespace inertia::fake;

namespace {

RenderOptions encrypt(bool value) {
    RenderOptions opts;
    opts.encryptHistory = value;
    return opts;
}

RenderOptions bigints(bool value) {
    RenderOptions opts;
    opts.preserveBigIntegers = value;
    return opts;
}

} // namespace

TEST_CASE("flash data is emitted once and merged", "[flash][protocol:page.flash]") {
    const Inertia app{Config{}};
    MemorySession session;
    flash(session, "ok", 1);
    flash(session, "n", 2);
    const auto page = app.page(FakeRequest::inertia("/"), session, "C", {});
    REQUIRE(dumpJson(page["flash"]) == R"({"ok":1,"n":2})");
    REQUIRE_FALSE(app.page(FakeRequest::inertia("/"), session, "C", {}).contains("flash"));
}

TEST_CASE("clearHistory is emitted once", "[history][protocol:page.clear-history]") {
    const Inertia app{Config{}};
    MemorySession session;
    REQUIRE_FALSE(app.page(FakeRequest::inertia("/"), session, "C", {}).contains("clearHistory"));
    clearHistory(session);
    REQUIRE(app.page(FakeRequest::inertia("/"), session, "C", {})["clearHistory"] == true);
    REQUIRE_FALSE(app.page(FakeRequest::inertia("/"), session, "C", {}).contains("clearHistory"));
}

TEST_CASE("preserveFragment is emitted once", "[history][protocol:page.preserve-fragment]") {
    const Inertia app{Config{}};
    MemorySession session;
    preserveFragment(session);
    REQUIRE(app.page(FakeRequest::inertia("/"), session, "C", {})["preserveFragment"] == true);
    REQUIRE_FALSE(
        app.page(FakeRequest::inertia("/"), session, "C", {}).contains("preserveFragment"));
}

TEST_CASE("encryptHistory follows config and override",
          "[history][protocol:page.encrypt-history]") {
    MemorySession session;
    const auto req = FakeRequest::inertia("/");
    REQUIRE_FALSE(Inertia{Config{}}.page(req, session, "C", {}).contains("encryptHistory"));
    Config config;
    config.encryptHistory = true;
    const Inertia app{config};
    REQUIRE(app.page(req, session, "C", {})["encryptHistory"] == true);
    REQUIRE_FALSE(app.page(req, session, "C", {}, encrypt(false)).contains("encryptHistory"));
    REQUIRE(Inertia{Config{}}.page(req, session, "C", {}, encrypt(true))["encryptHistory"] == true);
}

TEST_CASE("history flags keep protocol order", "[history]") {
    Config config;
    config.encryptHistory = true;
    config.preserveBigIntegers = true;
    const Inertia app{config};
    MemorySession session;
    clearHistory(session);
    preserveFragment(session);
    flash(session, "k", 1);
    const auto page = app.page(FakeRequest::inertia("/"), session, "C", {});
    REQUIRE(dumpJson(page) ==
            R"({"component":"C","props":{"errors":{}},"url":"/","version":"",)"
            R"("clearHistory":true,"encryptHistory":true,"preserveFragment":true,)"
            R"("preserveBigIntegers":true,"flash":{"k":1}})");
}

TEST_CASE("big integers are preserved when enabled", "[bigint][protocol:page.bigint]") {
    Config config;
    config.preserveBigIntegers = true;
    const Inertia app{config};
    MemorySession session;
    const std::int64_t big = 9007199254740993LL;
    const std::uint64_t huge = 18446744073709551615ULL;
    flash(session, "f", big);
    const auto page = app.page(FakeRequest::inertia("/"), session, "C",
                               {{"a", big},
                                {"b", 9007199254740991LL},
                                {"c", -big},
                                {"d", huge},
                                {"e", Json{{"n", Json::array({big})}}}});
    REQUIRE(page["preserveBigIntegers"] == true);
    REQUIRE(dumpJson(page["props"]) ==
            R"({"a":{"$bigint":"9007199254740993"},"b":9007199254740991,)"
            R"("c":{"$bigint":"-9007199254740993"},"d":{"$bigint":"18446744073709551615"},)"
            R"("e":{"n":[{"$bigint":"9007199254740993"}]},"errors":{}})");
    REQUIRE(dumpJson(page["flash"]) == R"({"f":{"$bigint":"9007199254740993"}})");

    const Inertia off{Config{}};
    const auto plain = off.page(FakeRequest::inertia("/"), session, "C", {{"a", big}});
    REQUIRE(plain["props"]["a"] == big);
    REQUIRE_FALSE(plain.contains("preserveBigIntegers"));

    const auto over =
        off.page(FakeRequest::inertia("/"), session, "C", {{"a", big}}, bigints(true));
    REQUIRE(over["props"]["a"]["$bigint"] == "9007199254740993");
}
