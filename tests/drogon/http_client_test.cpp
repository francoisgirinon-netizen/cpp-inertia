#include <catch2/catch_test_macros.hpp>

#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>

#include <chrono>
#include <future>
#include <string>

using inertia::HttpResult;
using inertia::drogon::DrogonHttpClient;
using namespace std::chrono_literals;

namespace {

std::string base() { return "http://127.0.0.1:" + std::to_string(inertia::drogon_test::port()); }

template <typename Start> HttpResult await(const Start &start) {
    std::promise<HttpResult> promise;
    auto future = promise.get_future();
    start([&promise](HttpResult result) { promise.set_value(std::move(result)); });
    REQUIRE(future.wait_for(10s) == std::future_status::ready);
    return future.get();
}

} // namespace

TEST_CASE("DrogonHttpClient get returns status and body", "[drogon]") {
    DrogonHttpClient client;
    const auto result =
        await([&](auto done) { client.get(base() + "/ping", 5s, std::move(done)); });
    CHECK(result.status == 200);
    CHECK(result.body == "pong");
    CHECK_FALSE(result.error.has_value());
}

TEST_CASE("DrogonHttpClient post sends the JSON body", "[drogon]") {
    DrogonHttpClient client;
    const auto result =
        await([&](auto done) { client.post(base() + "/echo", R"({"a":1})", 5s, std::move(done)); });
    CHECK(result.status == 200);
    CHECK(result.body == R"({"a":1})");
    CHECK_FALSE(result.error.has_value());
}

TEST_CASE("DrogonHttpClient reuses one connection pool per origin", "[drogon]") {
    DrogonHttpClient client;
    for (int i = 0; i < 3; ++i) {
        const auto result =
            await([&](auto done) { client.get(base() + "/ping", 5s, std::move(done)); });
        CHECK(result.body == "pong");
    }
}

TEST_CASE("DrogonHttpClient reports a timeout", "[drogon]") {
    DrogonHttpClient client;
    const auto result =
        await([&](auto done) { client.get(base() + "/slow", 100ms, std::move(done)); });
    CHECK(result.status == 0);
    CHECK(result.error.has_value());

    // Drogon keeps a timed-out request (and the client that owns it) alive until the server
    // answers on its connection. Responses come back in order on that connection, so once a later
    // request completes the late reply has been consumed and dropped: the client is usable again,
    // `done` was not called a second time, and nothing is still in flight when the process exits.
    const auto next = await([&](auto done) { client.get(base() + "/ping", 5s, std::move(done)); });
    CHECK(next.status == 200);
    CHECK(next.body == "pong");
}

TEST_CASE("DrogonHttpClient is not blocked by a hung request after a timeout", "[drogon][hol]") {
    DrogonHttpClient client;
    const auto hung =
        await([&](auto done) { client.get(base() + "/slow", 100ms, std::move(done)); });
    REQUIRE(hung.status == 0);

    // The timed-out request still occupies its connection (Drogon cannot cancel it and does not
    // pipeline), so a follow-up on that connection would wait for the whole /slow delay.
    const auto start = std::chrono::steady_clock::now();
    const auto next = await([&](auto done) { client.get(base() + "/ping", 5s, std::move(done)); });
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(next.status == 200);
    CHECK(next.body == "pong");
    CHECK(elapsed < inertia::drogon_test::kSlowDelay / 2);
}

TEST_CASE("DrogonHttpClient reports a connection failure", "[drogon]") {
    DrogonHttpClient client;
    const auto result =
        await([&](auto done) { client.get("http://127.0.0.1:1/x", 2s, std::move(done)); });
    CHECK(result.status == 0);
    CHECK(result.error.has_value());
}

TEST_CASE("DrogonHttpClient rejects a malformed URL without throwing", "[drogon]") {
    DrogonHttpClient client;
    const auto result = await([&](auto done) { client.get("not a url", 1s, std::move(done)); });
    CHECK(result.status == 0);
    CHECK(result.error.has_value());
}
