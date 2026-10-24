#include <catch2/catch_test_macros.hpp>

#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>

#include <chrono>
#include <future>
#include <string>
#include <thread>

using inertia::HttpResult;
using inertia::drogon::DrogonHttpClient;
using namespace std::chrono_literals;

namespace {

std::string base() { return "http://127.0.0.1:" + std::to_string(inertia::drogon_test::port()); }

template <typename Start> HttpResult await(const Start &start) {
    std::promise<HttpResult> promise;
    auto future = promise.get_future();
    start([&promise](HttpResult result) { promise.set_value(std::move(result)); });
    // Longer than any request timeout used below (at most ten times the /slow delay).
    REQUIRE(future.wait_for(20 * inertia::drogon_test::kSlowDelay) == std::future_status::ready);
    return future.get();
}

// An evicted client lives on until the server answers its abandoned /slow request. Waiting for
// that late reply keeps it from being still in flight when the process exits (LeakSanitizer).
void waitForLateReply(std::chrono::steady_clock::time_point slowSentAt) {
    std::this_thread::sleep_until(slowSentAt + inertia::drogon_test::kSlowDelay + 300ms);
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
    const auto slowSentAt = std::chrono::steady_clock::now();
    const auto result =
        await([&](auto done) { client.get(base() + "/slow", 100ms, std::move(done)); });
    CHECK(result.status == 0);
    CHECK(result.error.has_value());

    // Drogon keeps a timed-out request (and the client that owns it) alive until the server
    // answers on its connection; the late reply is then dropped and `done` is not called a second
    // time. A request sent right away would be queued behind it on the same connection (Drogon
    // does not pipeline, depth 0), which is why DrogonHttpClient evicts the client after a
    // timeout: the next request completes on a fresh connection. The timeout here is generous
    // (ten times the /slow delay) so that, even if that regressed, the request would complete
    // once the late reply is consumed instead of timing out and leaving work in flight at exit.
    const auto next = await([&](auto done) {
        client.get(base() + "/ping", 10 * inertia::drogon_test::kSlowDelay, std::move(done));
    });
    CHECK(next.status == 200);
    CHECK(next.body == "pong");
    waitForLateReply(slowSentAt);
}

TEST_CASE("DrogonHttpClient is not blocked by a hung request after a timeout", "[drogon][hol]") {
    DrogonHttpClient client;
    const auto slowSentAt = std::chrono::steady_clock::now();
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
    waitForLateReply(slowSentAt);
}

TEST_CASE("DrogonHttpClient evicts at most once per timeout window", "[drogon][window]") {
    DrogonHttpClient client;
    const auto slowSentAt = std::chrono::steady_clock::now();
    // The first timeout (600 ms) evicts the client and opens a 600 ms window.
    const auto first =
        await([&](auto done) { client.get(base() + "/slow", 600ms, std::move(done)); });
    REQUIRE(first.status == 0);
    // A second timeout (after 100 ms) falls inside that window: it must not evict again.
    const auto secondSentAt = std::chrono::steady_clock::now();
    const auto second =
        await([&](auto done) { client.get(base() + "/slow", 100ms, std::move(done)); });
    REQUIRE(second.status == 0);

    // So this request is still queued behind the second /slow on the same connection.
    const auto start = std::chrono::steady_clock::now();
    const auto next = await([&](auto done) { client.get(base() + "/ping", 5s, std::move(done)); });
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(next.status == 200);
    CHECK(elapsed > inertia::drogon_test::kSlowDelay / 2);
    waitForLateReply(slowSentAt);
    waitForLateReply(secondSentAt);
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
