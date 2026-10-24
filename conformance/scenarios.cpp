// Adapter-agnostic protocol scenarios. Everything is observed through send(); session state is
// only ever set by an earlier request of the same scenario.
#include <catch2/catch_test_macros.hpp>

#include <inertia/conformance/harness.hpp>

#include <inertia/html.hpp>

#include <algorithm>
#include <string>
#include <string_view>
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

// Builds the partial-reload request headers for component `component`.
Headers partialHeaders(std::string component, std::string only = "", std::string except = "") {
    Headers headers{{"X-Inertia-Partial-Component", std::move(component)}};
    if (!only.empty()) {
        headers.emplace_back("X-Inertia-Partial-Data", std::move(only));
    }
    if (!except.empty()) {
        headers.emplace_back("X-Inertia-Partial-Except", std::move(except));
    }
    return headers;
}

// Sends an Inertia GET and returns the 200 JSON page.
Json visit(const std::string &url, Headers extra = {}) {
    const auto res = send(inertiaRequest(url, std::move(extra)));
    REQUIRE(res.status == 200);
    return res.json();
}

Json partial(const std::string &url, std::string only, std::string except = "",
             const std::string &component = "") {
    return visit(url, partialHeaders(component, std::move(only), std::move(except)));
}

std::string meta(const Json &page, const char *key) {
    return page.contains(key) ? dumpJson(page[key]) : "absent";
}

// The string entries of a metadata array, sorted; empty when the key is absent.
std::vector<std::string> list(const Json &page, const char *key) {
    std::vector<std::string> out;
    if (page.contains(key)) {
        for (const auto &item : page[key]) {
            out.push_back(item.get<std::string>());
        }
    }
    std::ranges::sort(out);
    return out;
}

using Strings = std::vector<std::string>;

bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

// The page object embedded in the first-visit HTML's data-page script element.
Json pageFromHtml(const std::string &body) {
    const std::string open = R"(<script data-page="app" type="application/json">)";
    const auto begin = body.find(open);
    REQUIRE(begin != std::string::npos);
    const auto start = begin + open.size();
    const auto end = body.find("</script>", start);
    REQUIRE(end != std::string::npos);
    const Json page = Json::parse(body.substr(start, end - start), nullptr, false);
    REQUIRE_FALSE(page.is_discarded());
    return page;
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

// -- Partial reloads -----------------------------------------------------------------------------

TEST_CASE("only selects a nested path and its ancestors", "[conformance][protocol:partial.only]") {
    const auto props = partial("/partial", "auth.user", "", "Partial")["props"];
    REQUIRE(dumpJson(props["auth"]) == R"({"user":{"id":1}})");
    REQUIRE_FALSE(props.contains("other"));
}

TEST_CASE("only with an ancestor path selects the whole subtree",
          "[conformance][protocol:partial.only]") {
    const auto props = partial("/partial", "auth", "", "Partial")["props"];
    REQUIRE(dumpJson(props["auth"]) == R"({"user":{"id":1},"token":"t"})");
}

TEST_CASE("only accepts several comma separated paths", "[conformance][protocol:partial.only]") {
    const auto props = partial("/partial", "auth.token, other", "", "Partial")["props"];
    REQUIRE(dumpJson(props["auth"]) == R"({"token":"t"})");
    REQUIRE(props["other"] == 2);
}

TEST_CASE("except removes a path and keeps its siblings",
          "[conformance][protocol:partial.except]") {
    const auto props = partial("/partial", "", "auth.token", "Partial")["props"];
    REQUIRE(dumpJson(props["auth"]) == R"({"user":{"id":1}})");
    REQUIRE(props["other"] == 2);
}

TEST_CASE("except wins over only", "[conformance][protocol:partial.except]") {
    const auto whole = partial("/partial", "auth", "auth", "Partial")["props"];
    REQUIRE_FALSE(whole.contains("auth"));
    const auto token = partial("/partial", "auth", "auth.token", "Partial")["props"];
    REQUIRE(dumpJson(token["auth"]) == R"({"user":{"id":1}})");
}

TEST_CASE("a partial request for another component is a full visit",
          "[conformance][protocol:partial.component-match]") {
    const auto props = partial("/partial", "other", "", "Other")["props"];
    REQUIRE(props["auth"]["token"] == "t");
    REQUIRE(props["other"] == 2);
    REQUIRE_FALSE(props.contains("lazy"));
}

TEST_CASE("partial headers without the component header are ignored",
          "[conformance][protocol:partial.component-match]") {
    const auto page = visit("/partial", {{"X-Inertia-Partial-Data", "other"}});
    REQUIRE(page["props"]["auth"]["token"] == "t");
}

TEST_CASE("children of a resolved closure bypass the filter",
          "[conformance][protocol:partial.resolved-children]") {
    const auto props = partial("/partial", "res.x", "", "Partial")["props"];
    REQUIRE(dumpJson(props["res"]) == R"({"x":1,"y":2})");
}

TEST_CASE("top-level dot keys unpack into nested objects and filter by path",
          "[conformance][protocol:partial.dot-notation]") {
    REQUIRE(dumpJson(visit("/partial")["props"]["dot"]) == R"({"a":1,"b":2})");
    const auto props = partial("/partial", "dot.a", "", "Partial")["props"];
    REQUIRE(dumpJson(props["dot"]) == R"({"a":1})");
}

TEST_CASE("always props are resolved on full and partial visits",
          "[conformance][protocol:always.partial]") {
    REQUIRE(visit("/partial")["props"]["keep"] == 1);
    REQUIRE(partial("/partial", "other", "", "Partial")["props"]["keep"] == 1);
    REQUIRE(partial("/partial", "", "keep", "Partial")["props"]["keep"] == 1);
}

TEST_CASE("optional props are skipped on a full visit", "[conformance][protocol:optional.full]") {
    const auto page = visit("/partial");
    REQUIRE_FALSE(page["props"].contains("lazy"));
    REQUIRE_FALSE(page.contains("deferredProps"));
    REQUIRE_FALSE(page.contains("mergeProps"));
}

TEST_CASE("optional props resolve when selected", "[conformance][protocol:optional.partial]") {
    REQUIRE(partial("/partial", "lazy", "", "Partial")["props"]["lazy"] == 5);
    REQUIRE_FALSE(partial("/partial", "other", "", "Partial")["props"].contains("lazy"));
}

// -- Deferred props ------------------------------------------------------------------------------

TEST_CASE("deferred props are listed by group and omitted from props",
          "[conformance][protocol:defer.groups]") {
    const auto page = visit("/deferred");
    REQUIRE(dumpJson(page["props"]["auth"]) == R"({"user":1})");
    for (const char *key : {"f", "g", "h"}) {
        REQUIRE_FALSE(page["props"].contains(key));
    }
    REQUIRE(dumpJson(page["deferredProps"]) ==
            R"({"default":["f","auth.notifications"],"side":["g","h"]})");
}

TEST_CASE("a selected deferred prop resolves and deferredProps is absent on partials",
          "[conformance][protocol:defer.partial]") {
    const auto page = partial("/deferred", "f", "", "Deferred");
    REQUIRE(page["props"]["f"] == 7);
    REQUIRE_FALSE(page.contains("deferredProps"));
}

TEST_CASE("a rescued deferred prop is omitted and listed in rescuedProps",
          "[conformance][protocol:defer.rescue]") {
    const auto page = partial("/rescue", "f,x", "", "Rescue");
    REQUIRE_FALSE(page["props"].contains("f"));
    REQUIRE(page["props"]["x"] == 1);
    REQUIRE(meta(page, "rescuedProps") == R"(["f"])");
}

// -- Merge props ---------------------------------------------------------------------------------

TEST_CASE("merge lists the prop in mergeProps", "[conformance][protocol:merge.append]") {
    const auto page = visit("/merge");
    REQUIRE(list(page, "mergeProps") == Strings{"dm", "k", "m", "n.data"});
}

TEST_CASE("prepend lists the prop in prependProps", "[conformance][protocol:merge.prepend]") {
    REQUIRE(list(visit("/merge"), "prependProps") == Strings{"p"});
}

TEST_CASE("deepMerge lists the prop in deepMergeProps", "[conformance][protocol:merge.deep]") {
    REQUIRE(list(visit("/merge"), "deepMergeProps") == Strings{"d"});
}

TEST_CASE("nested append paths are listed with their full path",
          "[conformance][protocol:merge.nested-paths]") {
    const auto mergeList = list(visit("/merge"), "mergeProps");
    REQUIRE(std::ranges::find(mergeList, "n.data") != mergeList.end());
}

TEST_CASE("matchOn keys are listed in matchPropsOn", "[conformance][protocol:merge.match-on]") {
    REQUIRE(list(visit("/merge"), "matchPropsOn") == Strings{"k.id", "n.data.id"});
}

TEST_CASE("X-Inertia-Reset suppresses merge metadata", "[conformance][protocol:merge.reset]") {
    const auto page = visit("/merge", {{"X-Inertia-Reset", "k"}});
    REQUIRE(list(page, "mergeProps") == Strings{"dm", "m", "n.data"});
    REQUIRE(list(page, "matchPropsOn") == Strings{"n.data.id"});
}

TEST_CASE("merge metadata follows the partial only/except filter",
          "[conformance][protocol:merge.partial-filter]") {
    REQUIRE(list(partial("/merge", "m", "", "Merge"), "mergeProps") == Strings{"m"});
    const auto excluded = list(partial("/merge", "m,k", "m", "Merge"), "mergeProps");
    REQUIRE(excluded == Strings{"k"});
}

TEST_CASE("a deferred merge prop emits merge metadata while deferred",
          "[conformance][protocol:defer.merge]") {
    const auto page = visit("/merge");
    REQUIRE(meta(page, "deferredProps") == R"({"default":["dm"]})");
    REQUIRE_FALSE(page["props"].contains("dm"));
    const auto mergeList = list(page, "mergeProps");
    REQUIRE(std::ranges::find(mergeList, "dm") != mergeList.end());
}

// -- Once props ----------------------------------------------------------------------------------

TEST_CASE("once props are listed in onceProps with an expiry in milliseconds",
          "[conformance][protocol:once.metadata]") {
    const auto page = visit("/once");
    REQUIRE(page["props"]["f"] == 1);
    const auto &entry = page["onceProps"]["f"];
    REQUIRE(entry["prop"] == "f");
    REQUIRE(entry["expiresAt"].is_number_integer());
    REQUIRE(entry["expiresAt"].get<long long>() > 1'000'000'000'000LL);
    REQUIRE(page["onceProps"]["k"]["expiresAt"].is_null());
}

TEST_CASE("the except header drops a loaded once prop but keeps its metadata",
          "[conformance][protocol:once.except-header]") {
    const auto page = visit("/once", {{"X-Inertia-Except-Once-Props", "f"}});
    REQUIRE_FALSE(page["props"].contains("f"));
    REQUIRE(page["onceProps"]["f"]["prop"] == "f");
}

TEST_CASE("the except header is ignored on the first HTML visit",
          "[conformance][protocol:once.except-header]") {
    const auto res = send(plainRequest("/once", "GET", {{"X-Inertia-Except-Once-Props", "f"}}));
    REQUIRE(res.status == 200);
    // The prop is still resolved into the embedded page object.
    const auto page = pageFromHtml(res.body);
    REQUIRE(page["props"]["f"] == 1);
    REQUIRE(page["onceProps"]["f"]["prop"] == "f");
}

TEST_CASE("partial reloads ignore the once except header",
          "[conformance][protocol:once.partial-ignores-header]") {
    auto headers = partialHeaders("Once", "f");
    headers.emplace_back("X-Inertia-Except-Once-Props", "f");
    REQUIRE(visit("/once", headers)["props"]["f"] == 1);
}

TEST_CASE("a fresh once prop is resolved despite the header",
          "[conformance][protocol:once.fresh]") {
    const auto page = visit("/once", {{"X-Inertia-Except-Once-Props", "h"}});
    REQUIRE(page["props"]["h"] == 3);
}

TEST_CASE("a custom once key is matched against the header",
          "[conformance][protocol:once.custom-key]") {
    const auto byKey = visit("/once", {{"X-Inertia-Except-Once-Props", "k"}});
    REQUIRE_FALSE(byKey["props"].contains("g"));
    REQUIRE(byKey["onceProps"]["k"]["prop"] == "g");
    const auto byPath = visit("/once", {{"X-Inertia-Except-Once-Props", "g"}});
    REQUIRE(byPath["props"]["g"] == 2);
}

TEST_CASE("a deferred once prop already loaded has no deferredProps entry",
          "[conformance][protocol:once.defer]") {
    REQUIRE(meta(visit("/once"), "deferredProps") == R"({"default":["d"]})");
    const auto loaded = visit("/once", {{"X-Inertia-Except-Once-Props", "d"}});
    REQUIRE(meta(loaded, "deferredProps") == "absent");
    REQUIRE(loaded["onceProps"]["d"]["prop"] == "d");
}

// -- Scroll props --------------------------------------------------------------------------------

TEST_CASE("scroll emits scrollProps and a merge label", "[conformance][protocol:scroll.metadata]") {
    const auto page = visit("/scroll");
    REQUIRE(
        meta(page, "scrollProps") ==
        R"({"f":{"pageName":"page","previousPage":null,"nextPage":2,"currentPage":1,"reset":false}})");
    REQUIRE(meta(page, "mergeProps") == R"(["f.data"])");
}

TEST_CASE("the prepend intent moves the scroll label to prependProps",
          "[conformance][protocol:scroll.merge-intent]") {
    const auto page = visit("/scroll", {{"X-Inertia-Infinite-Scroll-Merge-Intent", "prepend"}});
    REQUIRE(meta(page, "prependProps") == R"(["f.data"])");
    REQUIRE(meta(page, "mergeProps") == "absent");
}

TEST_CASE("reset flags scrollProps and drops the label", "[conformance][protocol:scroll.reset]") {
    const auto page = visit("/scroll", {{"X-Inertia-Reset", "f"}});
    REQUIRE(page["scrollProps"]["f"]["reset"] == true);
    REQUIRE(meta(page, "mergeProps") == "absent");
}

TEST_CASE("a deferred scroll prop emits no scrollProps until loaded",
          "[conformance][protocol:scroll.deferred]") {
    const auto page = visit("/scroll-deferred");
    REQUIRE(meta(page, "deferredProps") == R"({"default":["f"]})");
    REQUIRE(meta(page, "mergeProps") == R"(["f.data"])");
    REQUIRE(meta(page, "scrollProps") == "absent");

    const auto loaded = partial("/scroll-deferred", "f", "", "ScrollDeferred");
    REQUIRE(loaded["scrollProps"].contains("f"));
    REQUIRE(loaded["props"]["f"]["data"] == Json::array({1, 2}));
}

// -- Version -------------------------------------------------------------------------------------

TEST_CASE("a stale version on an Inertia GET gets 409 with the location to reload",
          "[conformance][protocol:version.mismatch]") {
    const auto res = send(inertiaRequest("/page?b=1", {{"X-Inertia-Version", "old"}}));
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Location").value_or("").ends_with("/page?b=1"));
    REQUIRE(res.header("X-Inertia-Version") == "v1");
    REQUIRE_FALSE(res.header("X-Inertia").has_value());
    REQUIRE(res.body.empty());
}

TEST_CASE("a matching version, or a first visit, never gets the version 409",
          "[conformance][protocol:version.mismatch]") {
    REQUIRE(send(inertiaRequest("/page")).status == 200);
    REQUIRE(send(plainRequest("/page", "GET", {{"X-Inertia-Version", "old"}})).status == 200);
}

TEST_CASE("non-GET requests never get the version 409", "[conformance][protocol:version.non-get]") {
    for (const char *verb : {"POST", "PUT", "PATCH", "DELETE"}) {
        INFO(verb);
        const auto res =
            send(inertiaRequest("/update-redirect", {{"X-Inertia-Version", "old"}}, verb));
        // The route's own redirect is returned: 302 for POST, rewritten to 303 for the others.
        REQUIRE(res.status == (std::string_view(verb) == "POST" ? 302 : 303));
        REQUIRE(res.header("Location").value_or("") == "/page");
        REQUIRE_FALSE(res.header("X-Inertia-Location").has_value());
    }
}

// -- Redirects and empty responses ---------------------------------------------------------------

TEST_CASE("302 becomes 303 for Inertia PUT, PATCH and DELETE but not POST",
          "[conformance][protocol:redirect.303]") {
    for (const char *verb : {"PUT", "PATCH", "DELETE"}) {
        INFO(verb);
        const auto res = send(inertiaRequest("/update-redirect", {}, verb));
        REQUIRE(res.status == 303);
        REQUIRE(res.header("Location") == "/page");
    }
    REQUIRE(send(inertiaRequest("/update-redirect", {}, "POST")).status == 302);
}

TEST_CASE("302 stays 302 for a non-Inertia PUT", "[conformance][protocol:redirect.303]") {
    const auto res = send(plainRequest("/update-redirect", "PUT"));
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location") == "/page");
}

TEST_CASE("an Inertia redirect with a fragment becomes 409 with X-Inertia-Redirect",
          "[conformance][protocol:redirect.fragment]") {
    reset();
    const auto res = send(inertiaRequest("/fragment", {}, "POST"));
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Redirect") == "/page#section");
    REQUIRE_FALSE(res.header("Location").has_value());
    REQUIRE_FALSE(res.header("X-Inertia").has_value());
    REQUIRE(res.body.empty());
}

TEST_CASE("a fragment redirect keeps its status for prefetch and non-Inertia requests",
          "[conformance][protocol:redirect.fragment]") {
    reset();
    const auto prefetch = send(inertiaRequest("/fragment", {{"Purpose", "prefetch"}}, "POST"));
    REQUIRE(prefetch.status == 302);
    REQUIRE(prefetch.header("Location") == "/page#section");
    const auto plain = send(plainRequest("/fragment", "POST"));
    REQUIRE(plain.status == 302);
    REQUIRE(plain.header("Location") == "/page#section");
}

TEST_CASE("an external location is a 409 for Inertia and a 302 otherwise",
          "[conformance][protocol:redirect.external]") {
    const auto inertia = send(inertiaRequest("/external"));
    REQUIRE(inertia.status == 409);
    REQUIRE(inertia.header("X-Inertia-Location") == "https://example.com/away");
    REQUIRE_FALSE(inertia.header("X-Inertia").has_value());
    REQUIRE(inertia.body.empty());

    const auto plain = send(plainRequest("/external"));
    REQUIRE(plain.status == 302);
    REQUIRE(plain.header("Location") == "https://example.com/away");
}

TEST_CASE("an empty Inertia 200 redirects to the Referer, or to / without one",
          "[conformance][protocol:response.empty]") {
    const auto withReferer = send(inertiaRequest("/empty", {{"Referer", "http://localhost/prev"}}));
    REQUIRE(withReferer.status == 302);
    REQUIRE(withReferer.header("Location") == "http://localhost/prev");

    const auto without = send(inertiaRequest("/empty"));
    REQUIRE(without.status == 302);
    REQUIRE(without.header("Location") == "/");
}

TEST_CASE("a body of \"0\" is not an empty response", "[conformance][protocol:response.empty]") {
    const auto res = send(inertiaRequest("/zero"));
    REQUIRE(res.status == 200);
    REQUIRE(res.body == "0");
}

TEST_CASE("an empty non-Inertia 200 is left alone", "[conformance][protocol:response.empty]") {
    REQUIRE(send(plainRequest("/empty")).status == 200);
}

// -- Session-backed state: errors, flash and history flags ---------------------------------------

TEST_CASE("errors is an empty object without validation errors",
          "[conformance][protocol:errors.empty]") {
    reset();
    const auto errors = visit("/form")["props"]["errors"];
    REQUIRE(errors.is_object());
    REQUIRE(errors.empty());
}

TEST_CASE("validation errors map to the first message and are consumed once",
          "[conformance][protocol:errors.first-message]") {
    reset();
    const auto stored = send(inertiaRequest("/errors", {}, "POST"));
    REQUIRE(stored.status == 302);
    REQUIRE(stored.header("Location") == "/form");
    REQUIRE(dumpJson(visit("/form")["props"]["errors"]) == R"({"name":"req"})");
    REQUIRE(visit("/form")["props"]["errors"].empty());
}

TEST_CASE("validation errors are scoped by the error bag header",
          "[conformance][protocol:errors.bag]") {
    reset();
    send(inertiaRequest("/errors", {}, "POST"));
    const auto page = visit("/form", {{"X-Inertia-Error-Bag", "login"}});
    REQUIRE(dumpJson(page["props"]["errors"]) == R"({"login":{"name":"req"}})");
}

TEST_CASE("flash data is emitted once and merged", "[conformance][protocol:page.flash]") {
    reset();
    REQUIRE_FALSE(visit("/form").contains("flash"));
    send(inertiaRequest("/flash", {}, "POST"));
    REQUIRE(dumpJson(visit("/form")["flash"]) == R"({"notice":"saved","n":2})");
    REQUIRE_FALSE(visit("/form").contains("flash"));
}

TEST_CASE("clearHistory is emitted once", "[conformance][protocol:page.clear-history]") {
    reset();
    REQUIRE_FALSE(visit("/page").contains("clearHistory"));
    send(inertiaRequest("/clear-history", {}, "POST"));
    REQUIRE(visit("/page")["clearHistory"] == true);
    REQUIRE_FALSE(visit("/page").contains("clearHistory"));
}

TEST_CASE("preserveFragment is emitted once", "[conformance][protocol:page.preserve-fragment]") {
    reset();
    REQUIRE_FALSE(visit("/page").contains("preserveFragment"));
    send(inertiaRequest("/fragment", {}, "POST"));
    REQUIRE(visit("/page")["preserveFragment"] == true);
    REQUIRE_FALSE(visit("/page").contains("preserveFragment"));
}

TEST_CASE("an unknown route is a 404", "[conformance]") {
    REQUIRE(send(plainRequest("/nowhere")).status == 404);
}
