#include <catch2/catch_test_macros.hpp>

#include "app.hpp"

#include <inertia/json.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using inertia::httplib_test::App;
using inertia::httplib_test::Request;
using namespace std::chrono_literals;

TEST_CASE("installing twice on one server throws", "[httplib]") {
    App app;
    CHECK_THROWS_AS(
        inertia::httplib::install(app.server().server(),
                                  std::make_shared<const inertia::Inertia>(inertia::Config{})),
        std::logic_error);
}

TEST_CASE("a server created at a recycled address can be installed again", "[httplib]") {
    for (int i = 0; i < 3; ++i) {
        auto server = std::make_unique<httplib::Server>();
        CHECK_NOTHROW(inertia::httplib::install(
            *server, std::make_shared<const inertia::Inertia>(inertia::Config{})));
    }
}

TEST_CASE("install rejects a null Inertia", "[httplib]") {
    httplib::Server server;
    CHECK_THROWS_AS(inertia::httplib::install(server, nullptr), std::invalid_argument);
}

TEST_CASE("the session cookie is HttpOnly, SameSite=Lax and Path=/", "[httplib]") {
    App app;
    auto client = app.client();
    const auto reply = client.get("/ping");
    const auto line = reply.setCookie("inertia_session");
    REQUIRE_FALSE(line.empty());
    CHECK(line.find("HttpOnly") != std::string::npos);
    CHECK(line.find("SameSite=Lax") != std::string::npos);
    CHECK(line.find("Path=/") != std::string::npos);
    CHECK(line.find("Secure") == std::string::npos);
    CHECK(reply.cookieValue("inertia_session").size() == 64);
    // Known session: the cookie is not re-sent.
    CHECK(client.get("/ping").setCookie("inertia_session").empty());
}

TEST_CASE("a forged session id is replaced, never adopted", "[httplib]") {
    App app;
    auto client = app.client();
    Request req;
    req.url = "/ping";
    const std::string forged(64, 'a');
    req.headers.emplace_back("Cookie", "inertia_session=" + forged);
    const auto reply = client.send(req);
    const auto issued = reply.cookieValue("inertia_session");
    CHECK_FALSE(issued.empty());
    CHECK(issued != forged);
}

TEST_CASE("sessions are isolated per client and keep their values", "[httplib]") {
    App app;
    auto first = app.client();
    auto second = app.client();
    CHECK(first.get("/session/get").body == "missing");
    CHECK(first.get(R"(/session/put?v={"a":[1,2],"b":"x"})").body == "stored");
    CHECK(first.get("/session/get").body == R"({"a":[1,2],"b":"x"})");
    CHECK(second.get("/session/get").body == "missing");

    CHECK(first.get("/session/pull").body == R"({"a":[1,2],"b":"x"})");
    CHECK(first.get("/session/pull").body == "missing");

    CHECK(first.get("/session/put?v=7").body == "stored");
    first.resetCookies();
    CHECK(first.get("/session/get").body == "missing");
}

TEST_CASE("an expired session starts over with a new cookie", "[httplib]") {
    auto options = App::defaultOptions();
    options.sessions = std::make_shared<inertia::httplib::MemorySessionStore>(
        inertia::httplib::MemorySessionOptions{.idleTtl = 1s});
    App app{std::move(options)};
    auto client = app.client();
    CHECK(client.get("/session/put?v=1").body == "stored");
    const auto before = client.cookies().at("inertia_session");
    std::this_thread::sleep_for(1300ms);
    CHECK(client.get("/session/get").body == "missing");
    CHECK(client.cookies().at("inertia_session") != before);
}

TEST_CASE("forwarded headers are ignored by default", "[httplib]") {
    App app;
    auto client = app.client();
    Request req;
    req.url = "/page";
    req.headers = {{"X-Inertia", "true"},
                   {"X-Inertia-Version", "stale"},
                   {"X-Forwarded-Proto", "https"},
                   {"X-Forwarded-Host", "app.example.com"}};
    const auto reply = client.send(req);
    CHECK(reply.header("X-Inertia-Location") ==
          "http://127.0.0.1:" + std::to_string(app.port()) + "/page");
    CHECK(reply.setCookie("inertia_session").find("Secure") == std::string::npos);
    CHECK(reply.setCookie("XSRF-TOKEN").find("Secure") == std::string::npos);
}

TEST_CASE("trusted forwarded headers shape fullUrl and mark cookies Secure", "[httplib]") {
    auto options = App::defaultOptions();
    options.trustForwardedHeaders = true;
    App app{std::move(options)};
    auto client = app.client();
    Request req;
    req.url = "/page";
    req.headers = {{"X-Inertia", "true"},
                   {"X-Inertia-Version", "stale"},
                   {"X-Forwarded-Proto", "https"},
                   {"X-Forwarded-Host", "app.example.com"}};
    const auto reply = client.send(req);
    CHECK(reply.header("X-Inertia-Location") == "https://app.example.com/page");
    CHECK(reply.setCookie("inertia_session").find("; Secure") != std::string::npos);
    CHECK(reply.setCookie("XSRF-TOKEN").find("; Secure") != std::string::npos);
}
