#include <catch2/catch_test_macros.hpp>

#include "test_server.hpp"

using inertia::drogon_test::Client;
using inertia::drogon_test::Request;

TEST_CASE("without sessions and with CSRF disabled, plain routes answer normally", "[drogon]") {
    Client client;
    const auto get = client.get("/ping");
    CHECK(get.status == 200);
    CHECK(get.body == "pong");

    Request post;
    post.method = "POST";
    post.url = "/ping";
    CHECK(client.send(post).status == 200);
}
