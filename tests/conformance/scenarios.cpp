// Adapter-agnostic protocol scenarios. Everything is observed through send(); session state is
// only ever set by an earlier request of the same scenario.
#include <catch2/catch_test_macros.hpp>

#include "harness.hpp"

#include <inertia/html.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using inertia::dumpJson;
using inertia::Json;
using namespace inertia::conformance;

namespace {

using Headers = std::vector<std::pair<std::string, std::string>>;

TestRequest plainRequest(std::string url, std::string method = "GET", Headers headers = {}) {
    return {.method = std::move(method),
            .url = std::move(url),
            .headers = std::move(headers),
            .body = {}};
}

void reset() {
    const auto res = send(plainRequest("/__reset", "POST"));
    REQUIRE(res.status < 400);
}

TestRequest inertiaRequest(std::string url, Headers extra = {}, std::string method = "GET") {
    auto req = plainRequest(std::move(url), std::move(method),
                            {{"X-Inertia", "true"}, {"X-Inertia-Version", "v1"}});
    for (auto &header : extra) {
        // An extra header replaces the default of the same name.
        std::erase_if(req.headers, [&](const auto &h) { return h.first == header.first; });
        req.headers.push_back(std::move(header));
    }
    return req;
}

// Sends an Inertia GET and returns the 200 JSON page.
Json visit(const std::string &url, Headers extra = {}) {
    const auto res = send(inertiaRequest(url, std::move(extra)));
    REQUIRE(res.status == 200);
    return res.json();
}

std::string meta(const Json &page, const char *key) {
    return page.contains(key) ? dumpJson(page[key]) : "absent";
}

bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

// -- Page, HTML and JSON responses -------------------------------------------------------------

TEST_CASE("an Inertia request gets the page as JSON", "[conformance][protocol:json.response]") {
    const auto res = send(inertiaRequest("/page"));
    REQUIRE(res.status == 200);
    REQUIRE(res.header("X-Inertia") == "true");
    REQUIRE(res.header("Content-Type").value_or("").starts_with("application/json"));
    REQUIRE(res.json()["component"] == "Page");
}

TEST_CASE("a first visit gets HTML with the page script element",
          "[conformance][protocol:html.script-element]") {
    const auto res = send(plainRequest("/page"));
    REQUIRE(res.status == 200);
    REQUIRE(res.header("Content-Type").value_or("").starts_with("text/html"));
    REQUIRE_FALSE(res.header("X-Inertia").has_value());
    REQUIRE(contains(res.body, R"(<script data-page="app" type="application/json">)"));
    REQUIRE(contains(res.body, R"(</script><div id="app"></div>)"));
}

TEST_CASE("the root id names both the script data-page and the root div",
          "[conformance][protocol:html.root-id]") {
    const auto res = send(plainRequest("/page"));
    REQUIRE(contains(res.body, R"(data-page="app")"));
    REQUIRE(contains(res.body, R"(<div id="app">)"));
}

TEST_CASE("props cannot break out of the page script element",
          "[conformance][protocol:html.json-escaping]") {
    const auto res = send(plainRequest("/escape"));
    REQUIRE(res.status == 200);
    REQUIRE(contains(res.body, "\\u003C\\/script\\u003E\\u003C!--x--\\u003E"));
    REQUIRE_FALSE(contains(res.body, "<!--"));
    std::size_t closing = 0;
    for (auto pos = res.body.find("</script>"); pos != std::string::npos;
         pos = res.body.find("</script>", pos + 1)) {
        ++closing;
    }
    REQUIRE(closing == 1);
}

TEST_CASE("the page object always has component, props with errors, url and version",
          "[conformance][protocol:page.required]") {
    const auto page = visit("/page?a=1");
    REQUIRE(page["component"] == "Page");
    REQUIRE(page["props"]["title"] == "Home");
    REQUIRE(page["props"]["errors"] == Json::object());
    REQUIRE(page["url"] == "/page?a=1");
    REQUIRE(page["version"] == "v1");
}

TEST_CASE("the page url keeps the trailing slash and the query as received",
          "[conformance][protocol:page.url.trailing-slash]") {
    REQUIRE(visit("/slash/")["url"] == "/slash/");
    REQUIRE(visit("/slash/?a=%20b")["url"] == "/slash/?a=%20b");
}

TEST_CASE("shared props are listed in sharedProps with errors last",
          "[conformance][protocol:page.shared-props]") {
    const auto page = visit("/page");
    REQUIRE(page["props"]["app"] == "cpp-inertia");
    REQUIRE(meta(page, "sharedProps") == R"(["app","errors"])");
}

TEST_CASE("metadata keys are omitted when empty",
          "[conformance][protocol:page.metadata.omit-empty]") {
    const auto page = visit("/page");
    for (const char *key : {"mergeProps", "prependProps", "deepMergeProps", "matchPropsOn",
                            "deferredProps", "rescuedProps", "scrollProps", "onceProps"}) {
        INFO(key);
        REQUIRE_FALSE(page.contains(key));
    }
}

TEST_CASE("every response carries Vary: X-Inertia", "[conformance][protocol:vary.always]") {
    reset();
    REQUIRE(send(plainRequest("/page")).header("Vary") == "X-Inertia");
    REQUIRE(send(inertiaRequest("/page")).header("Vary") == "X-Inertia");
    REQUIRE(send(inertiaRequest("/page", {{"X-Inertia-Version", "old"}})).header("Vary") ==
            "X-Inertia");
    REQUIRE(send(plainRequest("/external")).header("Vary") == "X-Inertia");
}

TEST_CASE("Vary: X-Inertia is appended to an existing Vary value",
          "[conformance][protocol:vary.always]") {
    REQUIRE(send(plainRequest("/vary")).header("Vary") == "Accept-Encoding, X-Inertia");
}

TEST_CASE("encryptHistory is emitted only when enabled for the response",
          "[conformance][protocol:page.encrypt-history]") {
    REQUIRE_FALSE(visit("/page").contains("encryptHistory"));
    REQUIRE(visit("/encrypted")["encryptHistory"] == true);
}

TEST_CASE("big integers serialize as bigint objects when enabled for the response",
          "[conformance][protocol:page.bigint]") {
    const auto page = visit("/bigint");
    REQUIRE(page["preserveBigIntegers"] == true);
    REQUIRE(dumpJson(page["props"]["big"]) == R"({"$bigint":"9007199254740993"})");
    REQUIRE_FALSE(visit("/page").contains("preserveBigIntegers"));
}
