#include "test_server.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

using inertia::drogon_test::Client;
using inertia::drogon_test::Request;

namespace {

Request ping(std::string proto) {
    Request req;
    req.url = "/ping";
    if (!proto.empty()) {
        req.headers.emplace_back("X-Forwarded-Proto", std::move(proto));
    }
    return req;
}

} // namespace

TEST_CASE("trusting forwarded headers: the CSRF cookie is Secure when the proxy used https",
          "[drogon]") {
    Client client;
    const auto line = client.send(ping("https")).setCookie("XSRF-TOKEN");
    REQUIRE_FALSE(line.empty());
    CHECK(line.find("Secure") != std::string::npos);
}

TEST_CASE("trusting forwarded headers: the CSRF cookie is not Secure for http or no header",
          "[drogon]") {
    Client first;
    CHECK(first.send(ping("http")).setCookie("XSRF-TOKEN").find("Secure") == std::string::npos);
    Client second;
    const auto line = second.send(ping("")).setCookie("XSRF-TOKEN");
    REQUIRE_FALSE(line.empty());
    CHECK(line.find("Secure") == std::string::npos);
}

TEST_CASE("trusting forwarded headers: the 409 location uses the forwarded proto and host",
          "[drogon]") {
    Client client;
    Request req;
    req.url = "/page?a=1";
    req.headers = {{"X-Inertia", "true"},
                   {"X-Inertia-Version", "stale"},
                   {"X-Forwarded-Proto", "https"},
                   {"X-Forwarded-Host", "public.example"}};
    const auto reply = client.send(req);
    CHECK(reply.status == 409);
    CHECK(reply.header("X-Inertia-Location") == "https://public.example/page?a=1");
}
