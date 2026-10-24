#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/html.hpp>
#include <inertia/inertia.hpp>
#include <inertia/props.hpp>
#include <inertia/session.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

using namespace inertia;
using namespace inertia::fake;

TEST_CASE("errors is an empty object without errors", "[errors][protocol:errors.empty]") {
    const Inertia app{Config{}};
    MemorySession session;
    const auto page = app.page(FakeRequest::inertia("/"), session, "C", {{"a", 1}});
    REQUIRE(page["props"]["errors"].is_object());
    REQUIRE(dumpJson(page["props"]) == R"({"a":1,"errors":{}})");
}

TEST_CASE("session errors win over a user errors prop", "[errors]") {
    const Inertia app{Config{}};
    MemorySession session;
    withErrors(session, {{"name", {"req"}}});
    const auto page = app.page(FakeRequest::inertia("/"), session, "C", {{"errors", "mine"}});
    REQUIRE(dumpJson(page["props"]) == R"({"errors":{"name":"req"}})");
}

TEST_CASE("errors map to the first message or all of them",
          "[errors][protocol:errors.first-message]") {
    MemorySession session;
    withErrors(session, {{"name", {"req", "min"}}});
    const Inertia app{Config{}};
    REQUIRE(dumpJson(app.page(FakeRequest::inertia("/"), session, "C", {})["props"]["errors"]) ==
            R"({"name":"req"})");
    // consumed
    REQUIRE(dumpJson(app.page(FakeRequest::inertia("/"), session, "C", {})["props"]["errors"]) ==
            "{}");

    Config config;
    config.withAllErrors = true;
    const Inertia all{config};
    withErrors(session, {{"name", {"req", "min"}}});
    REQUIRE(dumpJson(all.page(FakeRequest::inertia("/"), session, "C", {})["props"]["errors"]) ==
            R"({"name":["req","min"]})");
}

TEST_CASE("errors are scoped by bag", "[errors][protocol:errors.bag]") {
    const Inertia app{Config{}};
    MemorySession session;
    withErrors(session, {{"name", {"req"}}});
    const auto req = FakeRequest::inertia("/").with("X-Inertia-Error-Bag", "login");
    REQUIRE(dumpJson(app.page(req, session, "C", {})["props"]["errors"]) ==
            R"({"login":{"name":"req"}})");

    withErrors(session, {{"name", {"req"}}}, "signup");
    REQUIRE(dumpJson(app.page(FakeRequest::inertia("/"), session, "C", {})["props"]["errors"]) ==
            R"({"signup":{"name":"req"}})");
}

TEST_CASE("an empty error bag header is treated as absent", "[errors][protocol:errors.bag]") {
    const Inertia app{Config{}};
    MemorySession session;
    withErrors(session, {{"name", {"req"}}});
    const auto req = FakeRequest::inertia("/").with("X-Inertia-Error-Bag", "");
    REQUIRE(dumpJson(app.page(req, session, "C", {})["props"]["errors"]) == R"({"name":"req"})");
}
