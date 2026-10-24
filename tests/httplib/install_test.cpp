#include "app.hpp"
#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <inertia/httplib/httplib.hpp>
#include <inertia/inertia.hpp>

#include <httplib.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
    const auto reply = client.get("/token");
    const auto line = reply.setCookie("inertia_session");
    REQUIRE_FALSE(line.empty());
    CHECK(line.find("HttpOnly") != std::string::npos);
    CHECK(line.find("SameSite=Lax") != std::string::npos);
    CHECK(line.find("Path=/") != std::string::npos);
    CHECK(line.find("Secure") == std::string::npos);
    CHECK(reply.cookieValue("inertia_session").size() == 64);
    // Known session: the cookie is not re-sent.
    CHECK(client.get("/token").setCookie("inertia_session").empty());
}

TEST_CASE("a forged session id is replaced, never adopted", "[httplib]") {
    App app;
    auto client = app.client();
    Request req;
    req.url = "/token";
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

    req.url = "/token";
    req.headers.erase(req.headers.begin(), req.headers.begin() + 2); // keep only X-Forwarded-*
    const auto cookies = client.send(req);
    CHECK_FALSE(cookies.setCookie("inertia_session").empty());
    CHECK(cookies.setCookie("inertia_session").find("Secure") == std::string::npos);
    CHECK(cookies.setCookie("XSRF-TOKEN").find("Secure") == std::string::npos);
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

    req.url = "/token";
    req.headers.erase(req.headers.begin(), req.headers.begin() + 2); // keep only X-Forwarded-*
    const auto cookies = client.send(req);
    CHECK(cookies.setCookie("inertia_session").find("; Secure") != std::string::npos);
    CHECK(cookies.setCookie("XSRF-TOKEN").find("; Secure") != std::string::npos);
}

TEST_CASE("a request that never touches the session gets no cookie and creates no session",
          "[httplib]") {
    App app;
    auto client = app.client();
    const auto ping = client.get("/ping");
    CHECK(ping.setCookie("inertia_session").empty());
    CHECK(ping.setCookie("XSRF-TOKEN").empty());
    const auto missing = client.get("/nowhere");
    CHECK(missing.status == 404);
    CHECK(missing.setCookie("inertia_session").empty());
    CHECK(missing.setCookie("XSRF-TOKEN").empty());
    // A stale-version 409 is finalized but is no reason to start a session either.
    Request req;
    req.url = "/page";
    req.headers = {{"X-Inertia", "true"}, {"X-Inertia-Version", "stale"}};
    const auto conflict = client.send(req);
    CHECK(conflict.status == 409);
    CHECK(conflict.setCookie("inertia_session").empty());
    CHECK(app.sessions()->size() == 0);
}

TEST_CASE("an exempt POST does not create a session either", "[httplib]") {
    App app;
    auto client = app.client();
    Request req;
    req.method = "POST";
    req.url = "/echo";
    req.body = "x";
    CHECK(client.send(req).status == 200);
    CHECK(app.sessions()->size() == 0);
}

TEST_CASE("rendering a page starts the session and hands out both cookies", "[httplib]") {
    App app;
    auto client = app.client();
    const auto reply = client.get("/page");
    CHECK_FALSE(reply.setCookie("inertia_session").empty());
    CHECK_FALSE(reply.setCookie("XSRF-TOKEN").empty());
    CHECK(app.sessions()->size() == 1);
}

TEST_CASE("regenerateSession keeps the data, retires the old id and sets a new cookie",
          "[httplib]") {
    App app;
    auto client = app.client();
    const auto first = client.get("/token");
    const auto oldId = first.cookieValue("inertia_session");
    const auto oldToken = first.cookieValue("XSRF-TOKEN");
    REQUIRE_FALSE(oldId.empty());

    const auto rotated = client.get("/regenerate");
    const auto newId = rotated.cookieValue("inertia_session");
    CHECK_FALSE(newId.empty());
    CHECK(newId != oldId);
    CHECK(rotated.setCookie("inertia_session").find("HttpOnly") != std::string::npos);
    const auto newToken = rotated.cookieValue("XSRF-TOKEN");
    CHECK_FALSE(newToken.empty());
    CHECK(newToken != oldToken); // the CSRF token is renewed with the session

    CHECK(client.get("/session/get").body == "7"); // data followed the session

    Request stale;
    stale.url = "/session/get";
    stale.headers.emplace_back("Cookie", "inertia_session=" + oldId);
    const auto replay = client.send(stale);
    CHECK(replay.body == "missing"); // the old id is dead
    CHECK(replay.cookieValue("inertia_session") != oldId);
}

TEST_CASE("regenerateSession on a session-less request still starts one", "[httplib]") {
    App app;
    auto client = app.client();
    const auto rotated = client.get("/regenerate");
    CHECK_FALSE(rotated.cookieValue("inertia_session").empty());
    CHECK(client.get("/session/get").body == "7");
}

TEST_CASE("user pre- and post-routing hooks run next to Inertia's", "[httplib]") {
    auto options = App::defaultOptions();
    options.preRouting = [](const httplib::Request &req, httplib::Response &res) {
        if (req.path == "/hooked") {
            res.status = 202;
            res.set_content("from the hook", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    };
    options.postRouting = [](const httplib::Request &, httplib::Response &res) {
        res.set_header("X-Post-Hook", "ran");
    };
    App app{std::move(options)};
    auto client = app.client();

    const auto hooked = client.get("/hooked");
    CHECK(hooked.status == 202);
    CHECK(hooked.body == "from the hook");
    CHECK(hooked.header("X-Post-Hook") == "ran");
    CHECK(hooked.header("Vary").find("X-Inertia") != std::string::npos); // finalize still last

    const auto plain = client.get("/ping");
    CHECK(plain.body == "pong");
    CHECK(plain.header("X-Post-Hook") == "ran");
    CHECK(plain.header("Vary").find("X-Inertia") != std::string::npos);
}

TEST_CASE("with user hooks installed CSRF and the version check still apply", "[httplib]") {
    auto options = App::defaultOptions();
    options.preRouting = [](const httplib::Request &, httplib::Response &) {
        return httplib::Server::HandlerResponse::Unhandled;
    };
    options.postRouting = [](const httplib::Request &, httplib::Response &) {};
    App app{std::move(options)};
    auto client = app.client();
    Request post;
    post.method = "POST";
    post.url = "/submit";
    CHECK(client.send(post).status == 419);
    Request stale;
    stale.url = "/page";
    stale.headers = {{"X-Inertia", "true"}, {"X-Inertia-Version", "stale"}};
    CHECK(client.send(stale).status == 409);
}

TEST_CASE("Inertia's own answers skip the user pre-routing hook", "[httplib]") {
    int calls = 0;
    auto options = App::defaultOptions();
    options.preRouting = [&calls](const httplib::Request &, httplib::Response &) {
        ++calls;
        return httplib::Server::HandlerResponse::Unhandled;
    };
    App app{std::move(options)};
    auto client = app.client();
    Request post;
    post.method = "POST";
    post.url = "/submit";
    CHECK(client.send(post).status == 419);
    CHECK(calls == 0);
    CHECK(client.get("/ping").status == 200);
    CHECK(calls == 1);
}

TEST_CASE("absolute-form and asterisk targets never reach url() as an origin", "[httplib]") {
    std::mutex mutex;
    std::vector<std::string> seen;
    auto options = App::defaultOptions();
    options.postRouting = [&](const httplib::Request &req, httplib::Response &) {
        const inertia::httplib::HttplibRequest view{req};
        const std::scoped_lock lock{mutex};
        seen.push_back(view.url() + " | " + view.fullUrl());
    };
    App app{std::move(options)};
    (void)inertia::httplib_test::rawExchange(
        app.port(), "GET http://evil.example/x?y=1 HTTP/1.1\r\nHost: real.test\r\n"
                    "Connection: close\r\n\r\n");
    (void)inertia::httplib_test::rawExchange(
        app.port(), "OPTIONS * HTTP/1.1\r\nHost: real.test\r\nConnection: close\r\n\r\n");
    const std::scoped_lock lock{mutex};
    REQUIRE(seen.size() == 2);
    CHECK(seen[0] == "/x?y=1 | http://real.test/x?y=1");
    CHECK(seen[1] == "* | http://real.test");
}
