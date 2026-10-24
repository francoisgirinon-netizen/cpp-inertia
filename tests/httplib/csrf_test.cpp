#include "app.hpp"
#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <inertia/httplib/httplib.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

using inertia::httplib_test::App;
using inertia::httplib_test::Reply;
using inertia::httplib_test::Request;

namespace {

Request post(std::string url, std::string token = "", std::string method = "POST") {
    Request req;
    req.method = std::move(method);
    req.url = std::move(url);
    if (!token.empty()) {
        req.headers.emplace_back("X-XSRF-TOKEN", std::move(token));
    }
    return req;
}

bool isHex(const std::string &text) {
    return std::ranges::all_of(
        text, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });
}

} // namespace

TEST_CASE("a GET sets an unpredictable XSRF-TOKEN cookie readable by scripts", "[httplib]") {
    App app;
    auto client = app.client();
    const auto reply = client.get("/token");
    const auto line = reply.setCookie("XSRF-TOKEN");
    REQUIRE_FALSE(line.empty());
    CHECK(line.find("HttpOnly") == std::string::npos);
    CHECK(line.find("Path=/") != std::string::npos);
    CHECK(line.find("SameSite=Lax") != std::string::npos);
    CHECK(line.find("Secure") == std::string::npos);
    const auto token = reply.cookieValue("XSRF-TOKEN");
    CHECK(token.size() == 64);
    CHECK(isHex(token));
}

TEST_CASE("tokens differ between sessions", "[httplib]") {
    App app;
    auto first = app.client();
    auto second = app.client();
    const auto a = first.get("/token").cookieValue("XSRF-TOKEN");
    const auto b = second.get("/token").cookieValue("XSRF-TOKEN");
    CHECK_FALSE(a.empty());
    CHECK(a != b);
}

TEST_CASE("the cookie is not re-sent while the client already holds the token", "[httplib]") {
    App app;
    auto client = app.client();
    CHECK_FALSE(client.get("/token").setCookie("XSRF-TOKEN").empty());
    CHECK(client.get("/token").setCookie("XSRF-TOKEN").empty());
}

TEST_CASE("a POST without the token header is rejected with 419", "[httplib]") {
    App app;
    auto client = app.client();
    (void)client.get("/token");
    CHECK(client.send(post("/submit")).status == 419);
}

TEST_CASE("a POST with a wrong token is rejected with 419", "[httplib]") {
    App app;
    auto client = app.client();
    const auto token = client.get("/token").cookieValue("XSRF-TOKEN");
    REQUIRE_FALSE(token.empty());
    auto wrong = token;
    wrong.back() = wrong.back() == '0' ? '1' : '0';
    CHECK(client.send(post("/submit", wrong)).status == 419);
    CHECK(client.send(post("/submit", token.substr(0, 10))).status == 419);
    CHECK(client.send(post("/submit", token + "0")).status == 419);
}

TEST_CASE("a POST with the matching token passes, for every mutating verb", "[httplib]") {
    App app;
    auto client = app.client();
    const auto token = client.get("/token").cookieValue("XSRF-TOKEN");
    REQUIRE_FALSE(token.empty());
    for (const char *verb : {"POST", "PUT", "PATCH", "DELETE"}) {
        const auto reply = client.send(post("/submit", token, verb));
        CHECK(reply.status == 200);
        CHECK(reply.body == "submitted");
    }
}

TEST_CASE("safe methods never need a token", "[httplib]") {
    App app;
    auto client = app.client();
    CHECK(client.get("/submit").status == 200);
    CHECK(client.send(post("/submit", "", "OPTIONS")).status != 419);
}

TEST_CASE("a token from another session is rejected", "[httplib]") {
    App app;
    auto owner = app.client();
    auto other = app.client();
    const auto stolen = owner.get("/token").cookieValue("XSRF-TOKEN");
    (void)other.get("/token");
    CHECK(other.send(post("/submit", stolen)).status == 419);
}

TEST_CASE("a first request that is a POST gets 419 plus a token that works on retry", "[httplib]") {
    App app;
    auto client = app.client();
    const auto rejected = client.send(post("/submit"));
    CHECK(rejected.status == 419);
    const auto token = rejected.cookieValue("XSRF-TOKEN");
    REQUIRE_FALSE(token.empty());
    CHECK(client.send(post("/submit", token)).status == 200);
}

TEST_CASE("a short-circuited 409 is finalized but starts no session", "[httplib]") {
    App app;
    auto client = app.client();
    Request req;
    req.url = "/page";
    req.headers = {{"X-Inertia", "true"}, {"X-Inertia-Version", "stale"}};
    const auto reply = client.send(req);
    CHECK(reply.status == 409);
    CHECK(reply.header("Vary").find("X-Inertia") != std::string::npos);
    CHECK(reply.cookieValue("XSRF-TOKEN").empty());
}

TEST_CASE("exceptPaths skip the CSRF check", "[httplib]") {
    App app;
    auto client = app.client();
    Request req = post("/echo");
    req.body = "x";
    req.headers.emplace_back("Content-Type", "text/plain");
    CHECK(client.send(req).status == 200);
}

TEST_CASE("a trailing * in exceptPaths exempts a prefix", "[httplib]") {
    auto options = App::defaultOptions();
    options.csrf.exceptPaths = {"/sub*"};
    App app{std::move(options)};
    auto client = app.client();
    CHECK(client.send(post("/submit")).status == 200);
    CHECK(client.send(post("/echo")).status == 419);
}

TEST_CASE("CSRF can be disabled, and then no cookie is sent", "[httplib]") {
    auto options = App::defaultOptions();
    options.csrf.enabled = false;
    App app{std::move(options)};
    auto client = app.client();
    const auto reply = client.get("/token");
    CHECK(reply.setCookie("XSRF-TOKEN").empty());
    CHECK(client.send(post("/submit")).status == 200);
}

TEST_CASE("the cookie and header names are configurable", "[httplib]") {
    auto options = App::defaultOptions();
    options.csrf.cookie = "MY-TOKEN";
    options.csrf.header = "X-My-Token";
    App app{std::move(options)};
    auto client = app.client();
    const auto token = client.get("/token").cookieValue("MY-TOKEN");
    REQUIRE_FALSE(token.empty());
    Request req = post("/submit");
    req.headers.emplace_back("X-My-Token", token);
    CHECK(client.send(req).status == 200);
    CHECK(client.send(post("/submit", token)).status == 419);
}
