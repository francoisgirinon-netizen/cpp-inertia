#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>

#include <stdexcept>
#include <string>

using namespace inertia;
using namespace inertia::fake;

namespace {

Json fullPage(const Props &props) {
    const Inertia app{Config{}};
    MemorySession session;
    return app.page(FakeRequest::inertia("/"), session, "Page", props);
}

Json partialPage(const Props &props, const std::string &only) {
    const Inertia app{Config{}};
    MemorySession session;
    auto req = FakeRequest::inertia("/");
    req.with("X-Inertia-Partial-Component", "Page");
    req.with("X-Inertia-Partial-Data", only);
    return app.page(req, session, "Page", props);
}

} // namespace

TEST_CASE("deferred props are listed by group and never invoked",
          "[defer][protocol:defer.groups]") {
    int calls = 0;
    const auto fn = [&calls]() -> Prop {
        ++calls;
        return 1;
    };
    const auto page =
        fullPage({{"f", defer(fn)}, {"g", defer(fn, "side")}, {"h", defer(fn, "side")}});
    REQUIRE(calls == 0);
    REQUIRE(dumpJson(page["props"]) == R"({"errors":{}})");
    REQUIRE(dumpJson(page["deferredProps"]) == R"({"default":["f"],"side":["g","h"]})");
}

TEST_CASE("a nested deferred prop uses its full dot-path", "[defer][protocol:defer.groups]") {
    const auto page =
        fullPage({{"auth", object({{"user", 1}, {"notifications", defer([] { return 2; })}})}});
    REQUIRE(dumpJson(page["props"]) == R"({"auth":{"user":1},"errors":{}})");
    REQUIRE(dumpJson(page["deferredProps"]) == R"({"default":["auth.notifications"]})");
}

TEST_CASE("a selected deferred prop resolves and deferredProps is absent on partials",
          "[defer][protocol:defer.partial]") {
    const auto page = partialPage({{"f", defer([] { return 7; })}, {"x", 1}}, "f");
    REQUIRE(dumpJson(page["props"]) == R"({"f":7,"errors":{}})");
    REQUIRE_FALSE(page.contains("deferredProps"));
}

TEST_CASE("a rescued deferred prop is omitted and listed in rescuedProps",
          "[defer][protocol:defer.rescue]") {
    const auto page = partialPage(
        {{"f", defer([]() -> Prop { throw std::runtime_error("boom"); }).rescue()}, {"x", 1}},
        "f,x");
    REQUIRE(dumpJson(page["props"]) == R"({"x":1,"errors":{}})");
    REQUIRE(dumpJson(page["rescuedProps"]) == R"(["f"])");
}

TEST_CASE("a throwing deferred prop without rescue propagates", "[defer][protocol:defer.rescue]") {
    REQUIRE_THROWS_AS(
        partialPage({{"f", defer([]() -> Prop { throw std::runtime_error("boom"); })}}, "f"),
        std::runtime_error);
}
