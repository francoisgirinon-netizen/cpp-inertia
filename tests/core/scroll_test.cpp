#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>

#include <string>

using namespace inertia;
using namespace inertia::fake;

namespace {

Json render(const FakeRequest &req, const Props &props) {
    const Inertia app{Config{}};
    MemorySession session;
    return app.page(req, session, "Page", props);
}

Prop scrolled() {
    return scroll([] { return Json{{"data", Json::array({1, 2})}}; },
                  [] {
                      return ScrollMetadata{.pageName = "page",
                                            .previousPage = nullptr,
                                            .nextPage = 2,
                                            .currentPage = 1};
                  });
}

std::string meta(const Json &page, const char *key) {
    return page.contains(key) ? dumpJson(page[key]) : "absent";
}

} // namespace

TEST_CASE("scroll emits scrollProps and a merge label", "[scroll][protocol:scroll.metadata]") {
    const auto page = render(FakeRequest::inertia("/"), {{"f", scrolled()}});
    REQUIRE(
        meta(page, "scrollProps") ==
        R"({"f":{"pageName":"page","previousPage":null,"nextPage":2,"currentPage":1,"reset":false}})");
    REQUIRE(meta(page, "mergeProps") == R"(["f.data"])");
}

TEST_CASE("the prepend intent moves the label to prependProps",
          "[scroll][protocol:scroll.merge-intent]") {
    auto req = FakeRequest::inertia("/");
    req.with("X-Inertia-Infinite-Scroll-Merge-Intent", "prepend");
    const auto page = render(req, {{"f", scrolled()}});
    REQUIRE(meta(page, "prependProps") == R"(["f.data"])");
    REQUIRE(meta(page, "mergeProps") == "absent");
}

TEST_CASE("reset flags scrollProps and drops the label", "[scroll][protocol:scroll.reset]") {
    auto req = FakeRequest::inertia("/");
    req.with("X-Inertia-Reset", "f");
    const auto page = render(req, {{"f", scrolled()}});
    REQUIRE(page["scrollProps"]["f"]["reset"] == true);
    REQUIRE(meta(page, "mergeProps") == "absent");
}

TEST_CASE("a deferred scroll prop emits no scrollProps", "[scroll][protocol:scroll.deferred]") {
    int calls = 0;
    const Props props{{"f", scroll(
                                [&calls] {
                                    ++calls;
                                    return Json{{"data", Json::array({1})}};
                                },
                                [] { return ScrollMetadata{.nextPage = 2, .currentPage = 1}; })
                                .deferred()}};
    const auto page = render(FakeRequest::inertia("/"), props);
    REQUIRE(calls == 0);
    REQUIRE(meta(page, "deferredProps") == R"({"default":["f"]})");
    REQUIRE(meta(page, "mergeProps") == R"(["f.data"])");
    REQUIRE(meta(page, "scrollProps") == "absent");

    auto req = FakeRequest::inertia("/");
    req.with("X-Inertia-Partial-Component", "Page");
    req.with("X-Inertia-Partial-Data", "f");
    const auto partial = render(req, props);
    REQUIRE(calls == 1);
    REQUIRE(partial["scrollProps"].contains("f"));
    REQUIRE(partial["props"]["f"]["data"] == Json::array({1}));
}
