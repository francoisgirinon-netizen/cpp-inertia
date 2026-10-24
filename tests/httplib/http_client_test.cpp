#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <inertia/httplib/httplib.hpp>
#include <inertia/ports/http_client.hpp>

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <compare>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using inertia::HttpResult;
using inertia::httplib::HttplibHttpClient;
using inertia::httplib_test::TestServer;
using namespace std::chrono_literals;

namespace {

// /ping answers "pong", /echo echoes the body, /slow answers after 1 s.
class PeerServer : public TestServer {
  public:
    PeerServer() {
        auto &raw = server();
        raw.Get("/ping", [](const httplib::Request &, httplib::Response &res) {
            res.set_content("pong", "text/plain");
        });
        raw.Post("/echo", [](const httplib::Request &req, httplib::Response &res) {
            res.set_content(req.body, "text/plain");
        });
        raw.Get("/slow", [](const httplib::Request &, httplib::Response &res) {
            std::this_thread::sleep_for(1s);
            res.set_content("late", "text/plain");
        });
        raw.Get("/target", [](const httplib::Request &req, httplib::Response &res) {
            res.set_content(req.target, "text/plain");
        });
        raw.Get("/teapot", [](const httplib::Request &, httplib::Response &res) {
            res.status = 418;
            res.set_content("short and stout", "text/plain");
        });
        start();
    }
};

PeerServer &peer() {
    static PeerServer instance;
    return instance;
}

std::string base() { return peer().baseUrl(); }

template <typename Start> HttpResult await(const Start &start) {
    std::promise<HttpResult> promise;
    auto future = promise.get_future();
    start([&promise](HttpResult result) { promise.set_value(std::move(result)); });
    REQUIRE(future.wait_for(10s) == std::future_status::ready);
    return future.get();
}

} // namespace

TEST_CASE("HttplibHttpClient get returns status and body", "[httplib]") {
    HttplibHttpClient client;
    const auto result =
        await([&](auto done) { client.get(base() + "/ping", 5s, std::move(done)); });
    CHECK(result.status == 200);
    CHECK(result.body == "pong");
    CHECK_FALSE(result.error.has_value());
}

TEST_CASE("HttplibHttpClient get sends the path and query", "[httplib]") {
    HttplibHttpClient client;
    const auto result =
        await([&](auto done) { client.get(base() + "/target?a=1&b=2", 5s, std::move(done)); });
    CHECK(result.body == "/target?a=1&b=2"); // cpp-httplib normalizes the encoding itself
}

TEST_CASE("HttplibHttpClient post sends the JSON body", "[httplib]") {
    HttplibHttpClient client;
    const auto result =
        await([&](auto done) { client.post(base() + "/echo", R"({"a":1})", 5s, std::move(done)); });
    CHECK(result.status == 200);
    CHECK(result.body == R"({"a":1})");
    CHECK_FALSE(result.error.has_value());
}

TEST_CASE("HttplibHttpClient sends the body as application/json", "[httplib]") {
    // The echo route reports the Content-Type it saw in a header, which the client does not
    // expose, so go through the raw server instead.
    std::promise<std::string> seen;
    auto seenType = seen.get_future();
    TestServer spy;
    spy.server().Post("/seen", [&seen](const httplib::Request &req, httplib::Response &res) {
        seen.set_value(req.get_header_value("Content-Type"));
        res.set_content("ok", "text/plain");
    });
    spy.start();
    HttplibHttpClient client;
    const auto result =
        await([&](auto done) { client.post(spy.baseUrl() + "/seen", "{}", 5s, std::move(done)); });
    CHECK(result.status == 200);
    CHECK(seenType.get() == "application/json");
}

TEST_CASE("HttplibHttpClient reports non-2xx statuses without an error", "[httplib]") {
    HttplibHttpClient client;
    const auto result =
        await([&](auto done) { client.get(base() + "/teapot", 5s, std::move(done)); });
    CHECK(result.status == 418);
    CHECK(result.body == "short and stout");
    CHECK_FALSE(result.error.has_value());
}

TEST_CASE("HttplibHttpClient reports a timeout", "[httplib]") {
    HttplibHttpClient client;
    const auto started = std::chrono::steady_clock::now();
    const auto result =
        await([&](auto done) { client.get(base() + "/slow", 100ms, std::move(done)); });
    CHECK(result.status == 0);
    CHECK(result.error.has_value());
    CHECK(std::chrono::steady_clock::now() - started < 900ms);
}

TEST_CASE("HttplibHttpClient reports a connection failure", "[httplib]") {
    HttplibHttpClient client;
    const auto result =
        await([&](auto done) { client.get("http://127.0.0.1:1/x", 2s, std::move(done)); });
    CHECK(result.status == 0);
    CHECK(result.error.has_value());
}

TEST_CASE("HttplibHttpClient rejects a malformed URL without throwing", "[httplib]") {
    HttplibHttpClient client;
    for (const char *url : {"not a url", "", "http://", "://x/y", "http://bad host/x",
                            "http://user:pw@127.0.0.1/x"}) {
        const auto result = await([&](auto done) { client.get(url, 1s, std::move(done)); });
        CHECK(result.status == 0);
        CHECK(result.error.has_value());
    }
}

TEST_CASE("HttplibHttpClient refuses https, which this build cannot speak", "[httplib]") {
    HttplibHttpClient client;
    const auto result =
        await([&](auto done) { client.get("https://127.0.0.1:1/x", 1s, std::move(done)); });
    CHECK(result.status == 0);
    CHECK(result.error.has_value());
}

TEST_CASE("HttplibHttpClient calls done exactly once", "[httplib]") {
    HttplibHttpClient client;
    for (const auto &url :
         {base() + "/ping", std::string{"http://127.0.0.1:1/"}, std::string{"x"}}) {
        int calls = 0;
        client.get(url, 1s, [&calls](const HttpResult &) { ++calls; });
        CHECK(calls == 1); // blocking: done has run by the time get() returns
    }
}

TEST_CASE("HttplibHttpClient is usable from many threads at once", "[httplib]") {
    HttplibHttpClient client;
    std::atomic<int> ok{0};
    std::vector<std::thread> threads;
    threads.reserve(6);
    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 10; ++i) {
                client.get(base() + "/ping", 5s, [&ok](const HttpResult &result) {
                    if (result.status == 200 && result.body == "pong") {
                        ++ok;
                    }
                });
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    CHECK(ok == 60);
}

TEST_CASE("HttplibHttpClient accepts a URL whose authority is followed by a query", "[httplib]") {
    HttplibHttpClient client;
    const auto result = await([&](auto done) { client.get(base() + "?a=1", 5s, std::move(done)); });
    // The server has no "/" route: reaching it (404) proves the URL was split correctly.
    CHECK(result.status == 404);
    CHECK_FALSE(result.error.has_value());
}
