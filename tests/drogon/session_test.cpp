#include <catch2/catch_test_macros.hpp>

#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>

#include <stdexcept>

using inertia::drogon_test::Client;

TEST_CASE("DrogonSession rejects a null session with a clear error", "[drogon]") {
    CHECK_THROWS_AS(inertia::drogon::DrogonSession{nullptr}, std::invalid_argument);
}

TEST_CASE("DrogonSession put then get then pull", "[drogon]") {
    Client client;
    CHECK(client.get("/session/get").body == "missing");

    CHECK(client.get(R"(/session/put?v={"a":[1,2],"b":"x"})").body == "stored");
    CHECK(client.get("/session/get").body == R"({"a":[1,2],"b":"x"})");
    CHECK(client.get("/session/get").body == R"({"a":[1,2],"b":"x"})"); // get does not consume

    CHECK(client.get("/session/pull").body == R"({"a":[1,2],"b":"x"})");
    CHECK(client.get("/session/pull").body == "missing");
    CHECK(client.get("/session/get").body == "missing");
}

TEST_CASE("DrogonSession put replaces an existing value", "[drogon]") {
    Client client;
    CHECK(client.get("/session/put?v=1").body == "stored");
    CHECK(client.get("/session/put?v=2").body == "stored");
    CHECK(client.get("/session/get").body == "2");
}

TEST_CASE("DrogonSession is isolated per client", "[drogon]") {
    Client first;
    Client second;
    CHECK(first.get("/session/put?v=7").body == "stored");
    CHECK(second.get("/session/get").body == "missing");
    first.resetCookies();
    CHECK(first.get("/session/get").body == "missing");
}
