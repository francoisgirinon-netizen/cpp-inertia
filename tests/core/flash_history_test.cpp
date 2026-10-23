#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>
#include <inertia/session.hpp>

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
