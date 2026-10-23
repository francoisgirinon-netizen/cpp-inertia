#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/headers.hpp>

#include <chrono>
#include <string>
#include <vector>

using namespace inertia;
using namespace inertia::fake;

TEST_CASE("FakeRequest::inertia sets X-Inertia and version", "[fake]") {
    const auto request = FakeRequest::inertia("/users?page=2");
    REQUIRE(request.header("x-inertia").value_or("") == "true");
    REQUIRE(request.header("x-inertia-version").has_value());
    REQUIRE(request.fullUrl() == "http://localhost/users?page=2");
}

TEST_CASE("MemorySession pull erases", "[fake]") {
    MemorySession session;
    session.put("k", Json(1));

    const auto pulled = session.pull("k");
    REQUIRE(pulled.has_value());
    REQUIRE(pulled.value_or(Json()) == Json(1));
    REQUIRE_FALSE(session.get("k").has_value());
}

TEST_CASE("FakeHttpClient replays in order and records", "[fake]") {
    FakeHttpClient http;
    http.replies.push_back({.status = 200, .body = "one", .error = std::nullopt});
    http.replies.push_back({.status = 500, .body = "two", .error = std::nullopt});

    std::vector<HttpResult> got;
    const std::chrono::milliseconds timeout{1000};
    http.post("http://x/a", "{}", timeout,
              [&](HttpResult result) { got.push_back(std::move(result)); });
    http.get("http://x/b", timeout, [&](HttpResult result) { got.push_back(std::move(result)); });

    REQUIRE(got.size() == 2);
    REQUIRE(got[0].status == 200);
    REQUIRE(got[0].body == "one");
    REQUIRE(got[1].status == 500);
    REQUIRE(got[1].body == "two");
    REQUIRE(http.posts.size() == 1);
    REQUIRE(http.posts[0].first == "http://x/a");
    REQUIRE(http.posts[0].second == "{}");
    REQUIRE(http.gets == std::vector<std::string>{"http://x/b"});
}
