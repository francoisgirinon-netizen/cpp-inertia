#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>
#include <inertia/session.hpp>

#include <cstdint>
#include <string>

using namespace inertia;
using namespace inertia::fake;

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
