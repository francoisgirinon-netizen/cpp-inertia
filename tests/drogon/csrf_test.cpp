#include "test_server.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

using inertia::drogon_test::Client;
using inertia::drogon_test::Reply;
using inertia::drogon_test::Request;

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

TEST_CASE("a GET sets an unpredictable XSRF-TOKEN cookie readable by scripts", "[drogon]") {
    Client client;
    const auto reply = client.get("/ping");
    const auto line = reply.setCookie("XSRF-TOKEN");
    REQUIRE_FALSE(line.empty());
    CHECK(line.find("HttpOnly") == std::string::npos);
    CHECK(line.find("Path=/") != std::string::npos);
    const auto token = reply.cookieValue("XSRF-TOKEN");
    CHECK(token.size() == 64);
    CHECK(isHex(token));
}

TEST_CASE("by default X-Forwarded-Proto is ignored: the cookie is not Secure over plain http",
          "[drogon]") {
    Client client;
    Request req;
    req.url = "/ping";
    req.headers.emplace_back("X-Forwarded-Proto", "https");
    const auto line = client.send(req).setCookie("XSRF-TOKEN");
    REQUIRE_FALSE(line.empty());
    CHECK(line.find("Secure") == std::string::npos);
}

TEST_CASE("tokens differ between sessions", "[drogon]") {
    Client first;
    Client second;
    const auto a = first.get("/ping").cookieValue("XSRF-TOKEN");
    const auto b = second.get("/ping").cookieValue("XSRF-TOKEN");
    CHECK_FALSE(a.empty());
    CHECK(a != b);
}

TEST_CASE("the cookie is not re-sent while the client already holds the token", "[drogon]") {
    Client client;
    CHECK_FALSE(client.get("/ping").setCookie("XSRF-TOKEN").empty());
    CHECK(client.get("/ping").setCookie("XSRF-TOKEN").empty());
}

TEST_CASE("a POST without the token header is rejected with 419", "[drogon]") {
    Client client;
    (void)client.get("/ping");
    const auto reply = client.send(post("/submit"));
    CHECK(reply.status == 419);
}

TEST_CASE("a POST with a wrong token is rejected with 419", "[drogon]") {
    Client client;
    const auto token = client.get("/ping").cookieValue("XSRF-TOKEN");
    REQUIRE_FALSE(token.empty());
    auto wrong = token;
    wrong.back() = wrong.back() == '0' ? '1' : '0';
    CHECK(client.send(post("/submit", wrong)).status == 419);
    CHECK(client.send(post("/submit", token.substr(0, 10))).status == 419);
}

TEST_CASE("a POST with the matching token passes, for every mutating verb", "[drogon]") {
    Client client;
    const auto token = client.get("/ping").cookieValue("XSRF-TOKEN");
    REQUIRE_FALSE(token.empty());
    for (const char *verb : {"POST", "PUT", "PATCH", "DELETE"}) {
        const auto reply = client.send(post("/submit", token, verb));
        CHECK(reply.status == 200);
        CHECK(reply.body == "submitted");
    }
}

TEST_CASE("a token from another session is rejected", "[drogon]") {
    Client owner;
    Client other;
    const auto stolen = owner.get("/ping").cookieValue("XSRF-TOKEN");
    (void)other.get("/ping");
    CHECK(other.send(post("/submit", stolen)).status == 419);
}

TEST_CASE("a first request that is a POST gets 419 plus a token that works on retry", "[drogon]") {
    Client client;
    const auto rejected = client.send(post("/submit"));
    CHECK(rejected.status == 419);
    const auto token = rejected.cookieValue("XSRF-TOKEN");
    REQUIRE_FALSE(token.empty());
    CHECK(client.send(post("/submit", token)).status == 200);
}

TEST_CASE("a short-circuited 409 also carries the token cookie", "[drogon]") {
    Client client;
    Request req;
    req.url = "/page";
    req.headers = {{"X-Inertia", "true"}, {"X-Inertia-Version", "stale"}};
    const auto reply = client.send(req);
    CHECK(reply.status == 409);
    CHECK_FALSE(reply.cookieValue("XSRF-TOKEN").empty());
}

TEST_CASE("exceptPaths skip the CSRF check", "[drogon]") {
    Client client;
    Request req = post("/echo");
    req.body = "x";
    CHECK(client.send(req).status == 200);
}

TEST_CASE("a wildcard exceptPath skips the CSRF check for every path under it", "[drogon]") {
    Client client;
    const auto reply = client.send(post("/hook/ping"));
    CHECK(reply.status == 200);
    CHECK(reply.body == "hooked");
}
