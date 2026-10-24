#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/html.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <utility>

using namespace inertia;
using namespace inertia::fake;

namespace {

constexpr auto kNow = std::chrono::system_clock::time_point(std::chrono::milliseconds(1'000'000));

Json render(const FakeRequest &req, const Props &props) {
    Config config;
    config.clock = [] { return kNow; };
    const Inertia app{std::move(config)};
    MemorySession session;
    return app.page(req, session, "Page", props);
}

FakeRequest withExcept(FakeRequest req, const std::string &keys) {
    req.with("X-Inertia-Except-Once-Props", keys);
    return req;
}

FakeRequest partialOnly(FakeRequest req, const std::string &only) {
    req.with("X-Inertia-Partial-Component", "Page");
    req.with("X-Inertia-Partial-Data", only);
    return req;
}

std::string meta(const Json &page, const char *key) {
    return page.contains(key) ? dumpJson(page[key]) : "absent";
}

} // namespace

TEST_CASE("once emits onceProps with a null expiry", "[once][protocol:once.metadata]") {
    const auto page = render(FakeRequest::inertia("/"), {{"f", once([] { return 1; })}});
    REQUIRE(meta(page, "onceProps") == R"({"f":{"prop":"f","expiresAt":null}})");
    REQUIRE(page["props"]["f"] == 1);
}

TEST_CASE("once with a ttl emits expiresAt in milliseconds", "[once][protocol:once.metadata]") {
    const auto page = render(FakeRequest::inertia("/"),
                             {{"f", once([] { return 1; }).until(std::chrono::seconds(60))}});
    REQUIRE(meta(page, "onceProps") == R"({"f":{"prop":"f","expiresAt":1060000}})");
}

TEST_CASE("a custom once key is matched against the header", "[once][protocol:once.custom-key]") {
    int calls = 0;
    const Props props{{"f", once([&calls] {
                                ++calls;
                                return 1;
                            }).as("k")}};
    const auto first = render(FakeRequest::inertia("/"), props);
    REQUIRE(meta(first, "onceProps") == R"({"k":{"prop":"f","expiresAt":null}})");
    REQUIRE(calls == 1);

    const auto byKey = render(withExcept(FakeRequest::inertia("/"), "k"), props);
    REQUIRE(calls == 1);
    REQUIRE_FALSE(byKey["props"].contains("f"));
    REQUIRE(meta(byKey, "onceProps") == R"({"k":{"prop":"f","expiresAt":null}})");

    const auto byPath = render(withExcept(FakeRequest::inertia("/"), "f"), props);
    REQUIRE(calls == 2);
    REQUIRE(byPath["props"]["f"] == 1);
}

TEST_CASE("the except header drops a once prop on Inertia full visits",
          "[once][protocol:once.except-header]") {
    int calls = 0;
    const Props props{{"f", once([&calls] {
                           ++calls;
                           return 1;
                       })}};
    const auto page = render(withExcept(FakeRequest::inertia("/"), "f"), props);
    REQUIRE(calls == 0);
    REQUIRE_FALSE(page["props"].contains("f"));
    REQUIRE(meta(page, "onceProps") == R"({"f":{"prop":"f","expiresAt":null}})");
}

TEST_CASE("the except header is ignored on the first HTML visit",
          "[once][protocol:once.except-header]") {
    const auto page =
        render(withExcept(FakeRequest::get("/"), "f"), {{"f", once([] { return 1; })}});
    REQUIRE(page["props"]["f"] == 1);
}

TEST_CASE("partial reloads ignore the except header",
          "[once][protocol:once.partial-ignores-header]") {
    const auto page = render(partialOnly(withExcept(FakeRequest::inertia("/"), "f"), "f"),
                             {{"f", once([] { return 1; })}});
    REQUIRE(page["props"]["f"] == 1);
}

TEST_CASE("a fresh once prop is resolved despite the header", "[once][protocol:once.fresh]") {
    const auto page =
        render(withExcept(FakeRequest::inertia("/"), "f"), {{"f", once([] { return 1; }).fresh()}});
    REQUIRE(page["props"]["f"] == 1);
}

TEST_CASE("a deferred once prop already loaded has no deferredProps entry",
          "[once][protocol:once.defer]") {
    const Props props{{"f", defer([] { return 1; }).once()}};
    const auto loaded = render(withExcept(FakeRequest::inertia("/"), "f"), props);
    REQUIRE(meta(loaded, "deferredProps") == "absent");
    REQUIRE(meta(loaded, "onceProps") == R"({"f":{"prop":"f","expiresAt":null}})");
    REQUIRE_FALSE(loaded["props"].contains("f"));

    const auto fresh = render(FakeRequest::inertia("/"), props);
    REQUIRE(meta(fresh, "deferredProps") == R"({"default":["f"]})");
}
