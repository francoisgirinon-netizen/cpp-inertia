#include <catch2/catch_test_macros.hpp>

#include "app.hpp"

#include <inertia/json.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using inertia::httplib_test::App;
using inertia::httplib_test::Request;

// Run under the tsan preset: Inertia is immutable once serving, and the adapter shares only the
// installed instance, the session store and per-session state, so concurrent requests must not
// race.
TEST_CASE("concurrent requests all succeed with identical props", "[httplib]") {
    constexpr int kThreads = 8;
    constexpr int kRequestsPerThread = 40;

    App app;
    std::atomic<int> ok{0};
    std::atomic<int> differing{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            auto client = app.client(); // one session per thread
            Request req;
            req.url = "/page";
            req.headers = {{"X-Inertia", "true"}, {"X-Inertia-Version", "v1"}};
            for (int i = 0; i < kRequestsPerThread; ++i) {
                const auto reply = client.send(req);
                if (reply.status != 200) {
                    continue;
                }
                ++ok;
                const auto page = inertia::Json::parse(reply.body);
                if (page["props"]["title"] != "Home" || page["props"]["count"] != 1) {
                    ++differing;
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    CHECK(ok == kThreads * kRequestsPerThread);
    CHECK(differing == 0);
    CHECK(app.sessions()->size() == kThreads);
}

TEST_CASE("concurrent requests of one session agree on a single CSRF token", "[httplib]") {
    constexpr int kThreads = 8;
    constexpr int kRequestsPerThread = 20;

    App app;
    // Establish the session first; every thread then presents the same session cookie.
    auto first = app.client();
    const auto reply = first.get("/ping");
    const auto token = reply.cookieValue("XSRF-TOKEN");
    const auto sessionId = reply.cookieValue("inertia_session");
    REQUIRE_FALSE(token.empty());
    REQUIRE_FALSE(sessionId.empty());

    std::atomic<int> accepted{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            auto client = app.client();
            Request req;
            req.method = "POST";
            req.url = "/submit";
            std::string cookies = "inertia_session=";
            cookies.append(sessionId).append("; XSRF-TOKEN=").append(token);
            req.headers = {{"X-XSRF-TOKEN", token}, {"Cookie", std::move(cookies)}};
            for (int i = 0; i < kRequestsPerThread; ++i) {
                if (client.send(req).status == 200) {
                    ++accepted;
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    CHECK(accepted == kThreads * kRequestsPerThread);
    CHECK(app.sessions()->size() == 1);
}

TEST_CASE("many first requests racing for one fresh session id create separate sessions",
          "[httplib]") {
    constexpr int kThreads = 8;

    App app;
    std::atomic<int> withCookie{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            auto client = app.client();
            if (!client.get("/session/put?v=1").cookieValue("inertia_session").empty()) {
                ++withCookie;
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    CHECK(withCookie == kThreads);
    CHECK(app.sessions()->size() == kThreads);
}
