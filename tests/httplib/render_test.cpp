#include "app.hpp"
#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <inertia/httplib/httplib.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <inertia/httplib/detail/httplib_include.hpp>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using inertia::httplib_test::App;
using inertia::httplib_test::Request;
using PageJson = inertia::Json;

namespace {

Request inertiaGet(std::string url, std::string version = "v1") {
    Request req;
    req.url = std::move(url);
    req.headers = {{"X-Inertia", "true"}, {"X-Inertia-Version", std::move(version)}};
    return req;
}

} // namespace

TEST_CASE("render answers an Inertia request with the page object", "[httplib]") {
    App app;
    auto client = app.client();
    const auto reply = client.send(inertiaGet("/page?x=1&y=2"));
    CHECK(reply.status == 200);
    CHECK(reply.header("X-Inertia") == "true");
    CHECK(reply.header("Content-Type").find("application/json") != std::string::npos);
    CHECK(reply.header("Vary").find("X-Inertia") != std::string::npos);
    const auto page = PageJson::parse(reply.body);
    CHECK(page["component"] == "Page");
    CHECK(page["url"] == "/page?x=1&y=2");
    CHECK(page["props"]["title"] == "Home");
    CHECK(page["version"] == "v1");
}

TEST_CASE("the page url is the request target exactly as received", "[httplib]") {
    App app;
    // cpp-httplib's own client rewrites %20 to +, so speak HTTP by hand.
    const auto raw = inertia::httplib_test::rawExchange(
        app.port(),
        "GET /page?x=a%20b&y=%7e&x=%2F HTTP/1.1\r\nHost: raw.test\r\nConnection: close\r\n"
        "X-Inertia: true\r\nX-Inertia-Version: v1\r\n\r\n");
    const auto bodyAt = raw.find("\r\n\r\n");
    REQUIRE(bodyAt != std::string::npos);
    const auto page = PageJson::parse(raw.substr(bodyAt + 4));
    CHECK(page["url"] == "/page?x=a%20b&y=%7e&x=%2F");
}

TEST_CASE("render answers a first visit with HTML", "[httplib]") {
    App app;
    auto client = app.client();
    const auto reply = client.get("/page");
    CHECK(reply.status == 200);
    CHECK(reply.header("Content-Type").find("text/html") != std::string::npos);
    CHECK(reply.body.find("data-page") != std::string::npos);
    CHECK(reply.header("Vary").find("X-Inertia") != std::string::npos);
}

TEST_CASE("a stale asset version short-circuits with a 409 that is still finalized", "[httplib]") {
    App app;
    auto client = app.client();
    const auto reply = client.send(inertiaGet("/page", "stale"));
    CHECK(reply.status == 409);
    CHECK(reply.header("X-Inertia-Location") ==
          "http://127.0.0.1:" + std::to_string(app.port()) + "/page");
    CHECK(reply.header("Vary").find("X-Inertia") != std::string::npos);
}

TEST_CASE("finalize merges into an existing Vary header", "[httplib]") {
    App app;
    auto client = app.client();
    const auto vary = client.get("/vary").header("Vary");
    CHECK(vary.find("Accept-Encoding") != std::string::npos);
    CHECK(vary.find("X-Inertia") != std::string::npos);
}

TEST_CASE("location is a 409 for Inertia requests and a 302 otherwise", "[httplib]") {
    App app;
    auto client = app.client();
    const auto inertiaReply = client.send(inertiaGet("/external"));
    CHECK(inertiaReply.status == 409);
    CHECK(inertiaReply.header("X-Inertia-Location") == "https://example.com/away");

    const auto plain = client.get("/external");
    CHECK(plain.status == 302);
    CHECK(plain.header("Location") == "https://example.com/away");
}

TEST_CASE("a throwing prop yields 500 and the server keeps serving", "[httplib]") {
    App app;
    auto client = app.client();
    const auto failed = client.send(inertiaGet("/throws"));
    CHECK(failed.status == 500);
    CHECK(failed.header("Vary").find("X-Inertia") != std::string::npos);

    const auto after = client.send(inertiaGet("/page"));
    CHECK(after.status == 200);
    CHECK(PageJson::parse(after.body)["props"]["title"] == "Home");
}

TEST_CASE("an exception escaping a route handler is a 500 and the server keeps serving",
          "[httplib]") {
    App app;
    auto client = app.client();
    CHECK(client.get("/boom").status == 500);
    CHECK(client.get("/ping").body == "pong");
}

TEST_CASE("finalize edits to status and headers reach the wire", "[httplib]") {
    App app;
    auto client = app.client();
    // An empty 200 to an Inertia request becomes a redirect back to the Referer.
    auto req = inertiaGet("/empty");
    req.headers.emplace_back("Referer", "/came-from");
    const auto redirected = client.send(req);
    CHECK(redirected.status == 302);
    CHECK(redirected.header("Location") == "/came-from");
    CHECK(redirected.header("Vary").find("X-Inertia") != std::string::npos);

    // The same route is untouched for a plain request.
    CHECK(client.get("/empty").status == 200);
}

TEST_CASE("an unknown route is a finalized 404", "[httplib]") {
    App app;
    auto client = app.client();
    const auto reply = client.get("/nowhere");
    CHECK(reply.status == 404);
    CHECK(reply.header("Vary").find("X-Inertia") != std::string::npos);
}

TEST_CASE("render, location and session outside an installed server are programmer errors",
          "[httplib]") {
    httplib::Request req;
    httplib::Response res;
    CHECK_THROWS_AS(inertia::httplib::render(req, res, "X", inertia::Props{}), std::logic_error);
    CHECK_THROWS_AS(inertia::httplib::location(req, res, "/x"), std::logic_error);
    CHECK_THROWS_AS(inertia::httplib::session(res), std::logic_error);
}
