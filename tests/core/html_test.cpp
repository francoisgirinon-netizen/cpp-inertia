#include <catch2/catch_test_macros.hpp>

#include <inertia/html.hpp>
#include <inertia/json.hpp>

#include <functional>
#include <string>

using namespace inertia;

TEST_CASE("escapeForScript escapes slash and angle brackets",
          "[html][protocol:html.json-escaping]") {
    const Json page = {{"s", "</script><!--x-->"}};
    const auto out = escapeForScript(page);
    REQUIRE(out.find("</script>") == std::string::npos);
    REQUIRE(out.find("<!--") == std::string::npos);
    REQUIRE(out == "{\"s\":\"\\u003C\\/script\\u003E\\u003C!--x--\\u003E\"}");
}

TEST_CASE("escapeForScript escapes line separators", "[html]") {
    const Json page = {{"s", "a\xE2\x80\xA8"
                             "b\xE2\x80\xA9"}};
    REQUIRE(escapeForScript(page) == "{\"s\":\"a\\u2028b\\u2029\"}");
}

TEST_CASE("dumpJson replaces invalid UTF-8", "[html]") {
    const Json page = {{"s", std::string("\xff")}};
    std::string out;
    REQUIRE_NOTHROW(out = dumpJson(page));
    REQUIRE(out == "{\"s\":\"\xEF\xBF\xBD\"}");
}

TEST_CASE("inertiaBody layout", "[html][protocol:html.script-element]") {
    const Json page = {{"a", 1}};
    REQUIRE(
        inertiaBody(page, "app") ==
        R"(<script data-page="app" type="application/json">{"a":1}</script><div id="app"></div>)");
}

TEST_CASE("defaultRootTemplate wraps body", "[html]") {
    const Json page = Json::object();
    const auto out =
        defaultRootTemplate({.page = page, .head = "<H>", .body = "<B>", .viteTags = "<V>"});
    REQUIRE(
        out ==
        R"(<!DOCTYPE html><html><head><meta charset="utf-8"><V><H></head><body><B></body></html>)");
}
