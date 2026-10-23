#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>

#include <string>

using namespace inertia;
using namespace inertia::fake;

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
