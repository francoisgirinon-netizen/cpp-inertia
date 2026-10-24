// JSON library comparison: nlohmann::ordered_json (what inertia::Json is today) against
// Boost.JSON, on the same typical page: building it, serializing it, copying it, and how much of a
// full render() is spent in JSON. Boost.JSON is used here only, never by the library. Needs the
// vcpkg feature `benchmarks`; see benchmarks/CMakeLists.txt.
//
// Boost.JSON objects keep insertion order as long as nothing is erased, so a page built with them
// has the same key order as one built with ordered_json.

#include <boost/json.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <inertia/html.hpp>
#include <inertia/json.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "fixtures.hpp"

namespace {

namespace bj = boost::json;

// The operations the page builder needs, for each library.
struct NlohmannOps {
    using Value = inertia::Json;
    static Value object() { return Value::object(); }
    static Value array() { return Value::array(); }
    static void set(Value &object, std::string_view key, Value value) {
        object[std::string(key)] = std::move(value);
    }
    static void push(Value &array, Value value) { array.push_back(std::move(value)); }
    static Value text(std::string_view text) {
        Value out = std::string(text);
        return out;
    }
    static Value integer(long long number) {
        Value out = number;
        return out;
    }
    static Value real(double number) {
        Value out = number;
        return out;
    }
    static Value boolean(bool flag) {
        Value out = flag;
        return out;
    }
    static Value null() {
        Value out = nullptr;
        return out;
    }
};

struct BoostOps {
    using Value = bj::value;
    static Value object() { return bj::object(); }
    static Value array() { return bj::array(); }
    static void set(Value &object, std::string_view key, Value value) {
        object.as_object().emplace(key, std::move(value));
    }
    static void push(Value &array, Value value) { array.as_array().push_back(std::move(value)); }
    static Value text(std::string_view text) {
        Value out(text);
        return out;
    }
    static Value integer(long long number) {
        Value out(number);
        return out;
    }
    static Value real(double number) {
        Value out(number);
        return out;
    }
    static Value boolean(bool flag) {
        Value out(flag);
        return out;
    }
    static Value null() {
        Value out(nullptr);
        return out;
    }
};

template <typename Ops> Ops::Value user(int index) {
    const auto id = std::to_string(index);
    auto meta = Ops::object();
    Ops::set(meta, "logins", Ops::integer(index * 7));
    Ops::set(meta, "score", Ops::real(index * 1.5));
    auto tags = Ops::array();
    for (const char *tag : {"a", "b", "c"}) {
        Ops::push(tags, Ops::text(tag));
    }
    Ops::set(meta, "tags", std::move(tags));
    auto out = Ops::object();
    Ops::set(out, "id", Ops::integer(index));
    Ops::set(out, "name", Ops::text("User Number " + id));
    Ops::set(out, "email", Ops::text("user" + id + "@example.com"));
    Ops::set(out, "role", Ops::text(index % 3 != 0 ? "member" : "admin"));
    Ops::set(out, "active", Ops::boolean(index % 2 == 0));
    Ops::set(out, "created_at", Ops::text("2025-01-02T03:04:05Z"));
    Ops::set(out, "avatar", Ops::text("https://cdn.example.com/avatars/" + id + ".png"));
    Ops::set(out, "meta", std::move(meta));
    return out;
}

// About the typical page of request_bench (resolved props and the page envelope): a user, a
// 50-item list, nested objects, three 1 KB texts that need escaping, and a handful of small ones.
template <typename Ops> Ops::Value typicalPage() {
    const std::string longText(1024, 'x');
    auto props = Ops::object();
    Ops::set(props, "user", user<Ops>(1));
    auto users = Ops::array();
    for (int i = 0; i < 50; ++i) {
        Ops::push(users, user<Ops>(i));
    }
    Ops::set(props, "users", std::move(users));
    auto stats = Ops::object();
    Ops::set(stats, "posts", Ops::integer(123));
    Ops::set(stats, "comments", Ops::integer(4567));
    Ops::set(stats, "ratio", Ops::real(0.731));
    auto series = Ops::array();
    for (int i = 1; i <= 10; ++i) {
        Ops::push(series, Ops::integer(i));
    }
    Ops::set(stats, "series", std::move(series));
    Ops::set(props, "stats", std::move(stats));
    for (const char *key : {"bio", "notes", "description"}) {
        Ops::set(props, key, Ops::text(longText + "</script> a/b/c"));
    }
    auto settings = Ops::object();
    Ops::set(settings, "theme", Ops::text("dark"));
    Ops::set(settings, "locale", Ops::text("fr"));
    auto notifications = Ops::object();
    Ops::set(notifications, "email", Ops::boolean(true));
    Ops::set(notifications, "push", Ops::boolean(false));
    Ops::set(settings, "notifications", std::move(notifications));
    Ops::set(props, "settings", std::move(settings));
    auto pagination = Ops::object();
    Ops::set(pagination, "page", Ops::integer(1));
    Ops::set(pagination, "next", Ops::text("/users?page=2"));
    Ops::set(pagination, "prev", Ops::null());
    Ops::set(props, "pagination", std::move(pagination));
    for (int i = 0; i < 6; ++i) {
        Ops::set(props, "shared" + std::to_string(i), Ops::text("shared value"));
    }
    auto page = Ops::object();
    Ops::set(page, "component", Ops::text("Users/Index"));
    Ops::set(page, "props", std::move(props));
    Ops::set(page, "url", Ops::text("/users?page=1"));
    Ops::set(page, "version", Ops::text("0123456789abcdef"));
    return page;
}

} // namespace

TEST_CASE("json library comparison", "[.json]") {
    const auto nlohmannPage = typicalPage<NlohmannOps>();
    const auto boostPage = typicalPage<BoostOps>();
    // The libraries print some values differently, so the sizes are close, not
    // equal.
    const auto nlohmannSize = inertia::dumpJson(nlohmannPage).size();
    const auto boostSize = bj::serialize(boostPage).size();
    REQUIRE(nlohmannSize < boostSize + boostSize / 50);
    REQUIRE(boostSize < nlohmannSize + nlohmannSize / 50);

    BENCHMARK("nlohmann::ordered_json: build the page") { return typicalPage<NlohmannOps>(); };
    BENCHMARK("boost::json: build the page") { return typicalPage<BoostOps>(); };
    BENCHMARK("nlohmann::ordered_json: serialize the page") {
        return inertia::dumpJson(nlohmannPage);
    };
    BENCHMARK("boost::json: serialize the page") { return bj::serialize(boostPage); };
    BENCHMARK("nlohmann::ordered_json: copy the page") {
        inertia::Json copy = nlohmannPage;
        return copy;
    };
    BENCHMARK("boost::json: copy the page") {
        bj::value copy = boostPage;
        return copy;
    };
}
