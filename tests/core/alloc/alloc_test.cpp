// Allocation-count regression tests: upper bounds on the number of heap allocations of the request
// hot path, so that a change that makes it allocate much more (a copy of the props, a parse of the
// manifest, a reallocating JSON object) fails here instead of showing up in a benchmark.
//
// This executable replaces the global operator new to count allocations, so it is separate from
// the other tests. Counting is per thread and only inside countAllocations().
//
// The bounds are the measured counts plus a small margin. To update one after a change that
// legitimately allocates more (or less), set the bound to 0 and run this executable: the failed
// assertion prints the measured count; set the bound to that count plus about 15 %, and note the
// reason in the commit. Do not raise a bound to silence an unexplained increase.
//
// Under AddressSanitizer the sanitizer owns operator new, so the counter is not installed and the
// tests are skipped.

#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/html.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>

#include <cstdlib>
#include <functional>
#include <new>
#include <string>
#include <utility>
#include <vector>

#if defined(__SANITIZE_ADDRESS__)
#define INERTIA_TEST_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define INERTIA_TEST_ASAN 1
#endif
#endif

#ifndef INERTIA_TEST_ASAN

namespace {

thread_local bool tCounting = false;
thread_local std::size_t tAllocations = 0;

void *allocate(std::size_t size) {
    if (tCounting) {
        ++tAllocations;
    }
    if (void *memory = std::malloc(size != 0 ? size : 1)) {
        return memory;
    }
    throw std::bad_alloc();
}

void *allocateAligned(std::size_t size, std::align_val_t alignment) {
    if (tCounting) {
        ++tAllocations;
    }
    const auto align = static_cast<std::size_t>(alignment);
    // aligned_alloc wants a size that is a multiple of the alignment.
    if (void *memory = std::aligned_alloc(align, (size + align - 1) / align * align)) {
        return memory;
    }
    throw std::bad_alloc();
}

} // namespace

// The replacement allocation functions: they are found by the linker, not declared in a header.
void *operator new(std::size_t size) { return allocate(size); }
void *operator new[](std::size_t size) { return allocate(size); }
void *operator new(std::size_t size, std::align_val_t alignment) {
    return allocateAligned(size, alignment);
}
void *operator new[](std::size_t size, std::align_val_t alignment) {
    return allocateAligned(size, alignment);
}
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void *memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }

namespace {

// The number of allocations `body` makes on this thread.
std::size_t countAllocations(const std::function<void()> &body) {
    tAllocations = 0;
    tCounting = true;
    body();
    tCounting = false;
    return tAllocations;
}

} // namespace

#else

namespace {

std::size_t countAllocations(const std::function<void()> &body) {
    body();
    return 0;
}

} // namespace

#endif

using namespace inertia;
using namespace inertia::fake;

namespace {

void skipUnderAsan() {
#ifdef INERTIA_TEST_ASAN
    SKIP("AddressSanitizer owns operator new: allocations cannot be counted");
#endif
}

// A fixed set of props: scalars, nested objects, a 20-item list, texts that script escaping
// rewrites, and one closure, optional, deferred and merge prop.
Props fixedProps() {
    Json users = Json::array();
    for (int i = 0; i < 20; ++i) {
        users.push_back(Json{{"id", i},
                             {"name", "User " + std::to_string(i)},
                             {"email", "user" + std::to_string(i) + "@example.com"},
                             {"active", i % 2 == 0},
                             {"meta", {{"logins", i * 7}, {"tags", {"a", "b", "c"}}}}});
    }
    Props props;
    props.emplace_back("users", std::move(users));
    props.emplace_back("stats", Json{{"posts", 123}, {"series", {1, 2, 3, 4, 5}}});
    props.emplace_back(
        "settings", Json{{"theme", "dark"}, {"notifications", {{"email", true}, {"push", false}}}});
    props.emplace_back("bio", std::string(512, 'x') + " </script> a/b/c");
    props.emplace_back("title", "Users");
    props.emplace_back("count", 20);
    props.emplace_back("lazy", Prop([] { return Prop(Json{{"computed", 42}}); }));
    props.emplace_back("opt", optional([] { return Prop(Json("only on partial")); }));
    props.emplace_back("slow", defer([] { return Prop(Json::array({1, 2, 3})); }));
    props.emplace_back("feed", merge(Prop(Json::array({1, 2, 3}))));
    return props;
}

void shareFixed(Inertia &app) {
    app.share("appName", "demo");
    app.share("locale", "fr");
    app.share("auth", Json{{"user", {{"id", 1}, {"name", "Ada"}}}, {"permissions", {"a", "b"}}});
}

// A Json page with every character that script escaping rewrites.
Json escapingPage() {
    Json users = Json::array();
    for (int i = 0; i < 20; ++i) {
        users.push_back(
            Json{{"id", i}, {"url", "/users/" + std::to_string(i)}, {"html", "<b>x</b>"}});
    }
    return Json{{"component", "Users/Index"},
                {"props", {{"users", std::move(users)}, {"text", "a </script> b \xE2\x80\xA8 c"}}},
                {"url", "/users"},
                {"version", ""}};
}

} // namespace

TEST_CASE("the allocation counter counts", "[alloc]") {
    skipUnderAsan();
    CHECK(countAllocations([] {}) == 0);
    CHECK(countAllocations([] {
              const std::vector<int> numbers(100, 1);
              REQUIRE(numbers.size() == 100);
          }) == 1);
}

TEST_CASE("page() on a fixed props set stays under its allocation bound", "[alloc]") {
    skipUnderAsan();
    Inertia app{Config{}};
    shareFixed(app);
    const Props props = fixedProps();
    const auto req = FakeRequest::inertia("/users?page=1", "");
    MemorySession session;

    Json page;
    const auto allocations =
        countAllocations([&] { page = app.page(req, session, "Users/Index", props); });

    REQUIRE(page["props"].contains("users"));
    // Measured: see the header comment for how to update this.
    constexpr std::size_t kPageBound = 415; // measured 360
    CHECK(allocations <= kPageBound);
}

TEST_CASE("escapeForScript() stays under its allocation bound", "[alloc]") {
    skipUnderAsan();
    const Json page = escapingPage();

    std::string escaped;
    const auto allocations = countAllocations([&] { escaped = escapeForScript(page); });

    REQUIRE(escaped.find("\\u2028") != std::string::npos);
    constexpr std::size_t kEscapeBound = 13; // measured 11
    CHECK(allocations <= kEscapeBound);
}

TEST_CASE("inertiaBody() stays under its allocation bound", "[alloc]") {
    skipUnderAsan();
    const Json page = escapingPage();

    std::string body;
    const auto allocations = countAllocations([&] { body = inertiaBody(page, "app"); });

    REQUIRE(body.starts_with("<script data-page=\"app\""));
    constexpr std::size_t kBodyBound = 13; // measured 11
    CHECK(allocations <= kBodyBound);
}

TEST_CASE("render() of a first visit without SSR stays under its allocation bound", "[alloc]") {
    skipUnderAsan();
    Inertia app{Config{}};
    shareFixed(app);
    const auto req = FakeRequest::get("/users?page=1");
    MemorySession session;
    Props props = fixedProps();

    std::string body;
    const auto allocations = countAllocations([&] {
        app.render(req, session, "Users/Index", std::move(props),
                   [&](Response res) { body = std::move(res.body); });
    });

    REQUIRE(body.starts_with("<!DOCTYPE html>"));
    constexpr std::size_t kRenderBound = 140; // measured 122
    CHECK(allocations <= kRenderBound);
}
