#include <catch2/catch_test_macros.hpp>

#include "test_server.hpp"

#include <string>

using inertia::drogon_test::Client;
using inertia::drogon_test::Request;

TEST_CASE("without app().enableSession() every request fails fast and says why", "[drogon]") {
    Client client;
    const auto get = client.get("/ping");
    CHECK(get.status == 500);
    CHECK(get.body.find("enableSession") != std::string::npos);

    Request post;
    post.method = "POST";
    post.url = "/ping";
    const auto reply = client.send(post);
    CHECK(reply.status == 500); // not a confusing 419
    CHECK(reply.body.find("enableSession") != std::string::npos);
}
