#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>

#include <string>

using namespace inertia;
using namespace inertia::fake;

namespace {

Json fullPage(const Props &props, const std::string &reset = "") {
    const Inertia app{Config{}};
    MemorySession session;
    auto req = FakeRequest::inertia("/");
    if (!reset.empty()) {
        req.with("X-Inertia-Reset", reset);
    }
    return app.page(req, session, "Page", props);
}

std::string meta(const Json &page, const char *key) {
    return page.contains(key) ? dumpJson(page[key]) : "absent";
}

Props one(Prop p) { return {{"x", std::move(p)}}; }

} // namespace

TEST_CASE("merge lists the prop in mergeProps", "[merge][protocol:merge.append]") {
    const auto page = fullPage(one(merge(1)));
    REQUIRE(meta(page, "mergeProps") == R"(["x"])");
    REQUIRE(meta(page, "prependProps") == "absent");
    REQUIRE(meta(page, "deepMergeProps") == "absent");
}

TEST_CASE("prepend lists the prop in prependProps", "[merge][protocol:merge.prepend]") {
    const auto page = fullPage(one(merge(1).prepend()));
    REQUIRE(meta(page, "prependProps") == R"(["x"])");
    REQUIRE(meta(page, "mergeProps") == "absent");
}

TEST_CASE("deepMerge lists the prop in deepMergeProps", "[merge][protocol:merge.deep]") {
    const auto page = fullPage(one(deepMerge(1)));
    REQUIRE(meta(page, "deepMergeProps") == R"(["x"])");
    REQUIRE(meta(page, "mergeProps") == "absent");
}

TEST_CASE("a deferred merge prop emits merge metadata while deferred",
          "[merge][protocol:defer.merge]") {
    const auto page = fullPage(one(defer([] { return 1; }).merge()));
    REQUIRE(meta(page, "mergeProps") == R"(["x"])");
    REQUIRE(meta(page, "deferredProps") == R"({"default":["x"]})");
}

TEST_CASE("nested append and prepend paths", "[merge][protocol:merge.nested-paths]") {
    const auto page = fullPage(one(merge(1).append("data").prepend("meta")));
    REQUIRE(meta(page, "mergeProps") == R"(["x.data"])");
    REQUIRE(meta(page, "prependProps") == R"(["x.meta"])");
}

TEST_CASE("matchOn keys", "[merge][protocol:merge.match-on]") {
    REQUIRE(meta(fullPage(one(merge(1).matchOn("id"))), "matchPropsOn") == R"(["x.id"])");
    REQUIRE(meta(fullPage(one(merge(1).append("data", "id"))), "matchPropsOn") ==
            R"(["x.data.id"])");
    REQUIRE(meta(fullPage(one(deepMerge(1).matchOn("id"))), "matchPropsOn") == R"(["x.id"])");
}
