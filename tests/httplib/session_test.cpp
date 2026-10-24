#include <catch2/catch_test_macros.hpp>

#include <inertia/httplib/httplib.hpp>
#include <inertia/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

using inertia::httplib::HttplibSession;
using inertia::httplib::MemorySessionOptions;
using inertia::httplib::MemorySessionStore;
using namespace std::chrono_literals;

namespace {

// A clock the tests advance by hand.
struct ManualClock {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::function<std::chrono::steady_clock::time_point()> fn() {
        return [this] { return now; };
    }
};

bool isHex(const std::string &text) {
    return std::ranges::all_of(
        text, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });
}

} // namespace

TEST_CASE("session ids are 256-bit random hex and never repeat", "[httplib]") {
    MemorySessionStore store;
    std::set<std::string> seen;
    for (int i = 0; i < 200; ++i) {
        const auto opened = store.open(std::nullopt);
        CHECK(opened.created);
        CHECK(opened.id.size() == 64);
        CHECK(isHex(opened.id));
        seen.insert(opened.id);
    }
    CHECK(seen.size() == 200);
    CHECK(store.size() == 200);
}

TEST_CASE("a known id returns the same session, an unknown one is never adopted", "[httplib]") {
    MemorySessionStore store;
    const auto first = store.open(std::nullopt);
    const auto again = store.open(first.id);
    CHECK_FALSE(again.created);
    CHECK(again.id == first.id);
    CHECK(again.data == first.data);

    const std::string forged(64, 'a');
    const auto fixed = store.open(forged);
    CHECK(fixed.created);
    CHECK(fixed.id != forged); // no session fixation
    CHECK(store.size() == 2);
    CHECK(store.open("short").created);
}

TEST_CASE("a session expires after the idle TTL and each access renews it", "[httplib]") {
    ManualClock clock;
    MemorySessionStore store{MemorySessionOptions{.idleTtl = 10s, .clock = clock.fn()}};
    const auto first = store.open(std::nullopt);

    clock.now += 9s;
    CHECK_FALSE(store.open(first.id).created); // renewed at t = 9
    clock.now += 9s;
    CHECK_FALSE(store.open(first.id).created); // renewed at t = 18

    clock.now += 11s;
    const auto expired = store.open(first.id);
    CHECK(expired.created);
    CHECK(expired.id != first.id);
}

TEST_CASE("sweep drops idle sessions and keeps live ones", "[httplib]") {
    ManualClock clock;
    MemorySessionStore store{MemorySessionOptions{.idleTtl = 10s, .clock = clock.fn()}};
    const auto idle = store.open(std::nullopt);
    clock.now += 8s;
    const auto live = store.open(std::nullopt);
    clock.now += 4s; // idle is 12 s old, live 4 s
    store.sweep();
    CHECK(store.size() == 1);
    CHECK_FALSE(store.open(live.id).created);
    CHECK(store.open(idle.id).created);
}

TEST_CASE("opening a session drops the expired ones, cheaply and in order", "[httplib]") {
    ManualClock clock;
    MemorySessionStore store{MemorySessionOptions{.idleTtl = 10s, .clock = clock.fn()}};
    for (int i = 0; i < 5; ++i) {
        (void)store.open(std::nullopt);
    }
    clock.now += 20s;
    (void)store.open(std::nullopt);
    CHECK(store.size() == 1);
}

TEST_CASE("at the cap the least recently used session is evicted", "[httplib]") {
    ManualClock clock;
    MemorySessionStore store{
        MemorySessionOptions{.idleTtl = 1h, .maxSessions = 3, .clock = clock.fn()}};
    const auto a = store.open(std::nullopt);
    clock.now += 1s;
    const auto b = store.open(std::nullopt);
    clock.now += 1s;
    const auto c = store.open(std::nullopt);
    clock.now += 1s;
    CHECK_FALSE(store.open(a.id).created); // a is now the most recently used; b is the oldest
    const auto d = store.open(std::nullopt);
    CHECK(store.size() == 3);
    CHECK(store.open(b.id).created); // evicted (and evicts c in turn)
    CHECK(store.size() == 3);
    CHECK(store.open(d.id).created == false);
    CHECK(store.open(c.id).created); // c was the least recently used when b came back
}

TEST_CASE("the default cap is 100000 sessions", "[httplib]") {
    CHECK(MemorySessionOptions{}.maxSessions == 100'000);
}

TEST_CASE("rotate moves the data to a new id and invalidates the old one", "[httplib]") {
    MemorySessionStore store;
    const auto first = store.open(std::nullopt);
    HttplibSession{first.data}.put("user", 42);

    const auto rotated = store.rotate(first.id);
    CHECK(rotated.created);
    CHECK(rotated.id != first.id);
    CHECK(rotated.id.size() == 64);
    CHECK(rotated.data == first.data);
    CHECK(HttplibSession{rotated.data}.get("user") == 42);
    CHECK(store.size() == 1);

    const auto stale = store.open(first.id);
    CHECK(stale.created);
    CHECK(stale.data != first.data);
    CHECK_FALSE(store.open(rotated.id).created);
}

TEST_CASE("rotate of an unknown id just opens a fresh session", "[httplib]") {
    MemorySessionStore store;
    const auto fresh = store.rotate("nope");
    CHECK(fresh.created);
    CHECK(store.size() == 1);
}

TEST_CASE("the session cookie is HttpOnly, SameSite=Lax, Path=/ and Secure on demand",
          "[httplib]") {
    MemorySessionStore store;
    CHECK(store.cookieName() == "inertia_session");
    const auto plain = store.cookieHeader("abc", false);
    CHECK(plain == "inertia_session=abc; Path=/; HttpOnly; SameSite=Lax");
    CHECK(store.cookieHeader("abc", true) == plain + "; Secure");

    MemorySessionStore renamed{MemorySessionOptions{.cookieName = "sid"}};
    CHECK(renamed.cookieHeader("x", false).starts_with("sid=x;"));
}

TEST_CASE("HttplibSession put then get then pull", "[httplib]") {
    MemorySessionStore store;
    HttplibSession session{store.open(std::nullopt).data};
    CHECK_FALSE(session.get("k").has_value());

    session.put("k", inertia::Json{{"a", {1, 2}}, {"b", "x"}});
    CHECK(session.get("k") == inertia::Json{{"a", {1, 2}}, {"b", "x"}});
    CHECK(session.get("k").has_value()); // get does not consume

    CHECK(session.pull("k") == inertia::Json{{"a", {1, 2}}, {"b", "x"}});
    CHECK_FALSE(session.pull("k").has_value());
    CHECK_FALSE(session.get("k").has_value());
}

TEST_CASE("HttplibSession put replaces and sessions are isolated", "[httplib]") {
    MemorySessionStore store;
    HttplibSession first{store.open(std::nullopt).data};
    HttplibSession second{store.open(std::nullopt).data};
    first.put("k", 1);
    first.put("k", 2);
    CHECK(first.get("k") == 2);
    CHECK_FALSE(second.get("k").has_value());
}

TEST_CASE("two views of one session share its values", "[httplib]") {
    MemorySessionStore store;
    const auto opened = store.open(std::nullopt);
    HttplibSession writer{opened.data};
    const HttplibSession reader{store.open(opened.id).data};
    writer.put("flash", "hello");
    CHECK(reader.get("flash") == "hello");
}

TEST_CASE("getOrCreate runs the factory once even when sessions race", "[httplib]") {
    MemorySessionStore store;
    const auto opened = store.open(std::nullopt);
    std::atomic<int> created{0};
    std::vector<std::thread> threads;
    std::vector<std::string> results(8);
    threads.reserve(results.size());
    for (std::size_t i = 0; i < results.size(); ++i) {
        threads.emplace_back([&, i] {
            HttplibSession session{opened.data};
            results[i] = session
                             .getOrCreate("token",
                                          [&] {
                                              ++created;
                                              return inertia::Json("t" + std::to_string(i));
                                          })
                             .get<std::string>();
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    CHECK(created == 1);
    CHECK(std::ranges::all_of(results, [&](const auto &r) { return r == results.front(); }));
}

TEST_CASE("concurrent open and put on many sessions do not race", "[httplib]") {
    MemorySessionStore store{MemorySessionOptions{.idleTtl = 60s}};
    const auto shared = store.open(std::nullopt);
    std::vector<std::thread> threads;
    threads.reserve(8);
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < 200; ++i) {
                HttplibSession mine{store.open(std::nullopt).data};
                mine.put("n", i);
                HttplibSession common{store.open(shared.id).data};
                common.put("last", t);
                (void)common.get("last");
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    CHECK(store.size() == 1 + 8 * 200);
}
