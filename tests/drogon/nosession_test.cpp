#include <catch2/catch_test_macros.hpp>

#include "test_server.hpp"

#include <string>

using inertia::drogon_test::Client;
using inertia::drogon_test::Request;

// Sessions off, CSRF enabled (the default).
TEST_CASE("without sessions, safe plain routes still work", "[drogon]") {
    Client client;
    const auto reply = client.get("/ping");
    CHECK(reply.status == 200);
    CHECK(reply.body == "pong");
}

TEST_CASE("without sessions, a mutating request fails with a clear error, not a 419", "[drogon]") {
    Client client;
    Request post;
    post.method = "POST";
    post.url = "/ping";
    const auto reply = client.send(post);
    CHECK(reply.status == 500);
    CHECK(reply.body.find("enableSession") != std::string::npos);
}

TEST_CASE("without sessions, render answers a clear 500", "[drogon]") {
    Client client;
    const auto reply = client.get("/page");
    CHECK(reply.status == 500);
    CHECK(reply.body.find("enableSession") != std::string::npos);
}
