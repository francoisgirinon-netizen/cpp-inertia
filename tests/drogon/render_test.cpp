#include <catch2/catch_test_macros.hpp>

#include "test_server.hpp"

#include <inertia/json.hpp>

#include <string>

using PageJson = inertia::Json; // jsoncpp (via Drogon) owns a global ::Json namespace
using inertia::drogon_test::Client;
using inertia::drogon_test::Request;

namespace {

Request inertiaGet(std::string url, std::string version = "v1") {
    Request req;
    req.url = std::move(url);
    req.headers = {{"X-Inertia", "true"}, {"X-Inertia-Version", std::move(version)}};
    return req;
}

} // namespace

TEST_CASE("render answers an Inertia request with the page object", "[drogon]") {
    Client client;
    const auto reply = client.send(inertiaGet("/page?x=1"));
    CHECK(reply.status == 200);
    CHECK(reply.header("X-Inertia") == "true");
    CHECK(reply.header("Vary").find("X-Inertia") != std::string::npos);
    const auto page = PageJson::parse(reply.body);
    CHECK(page["component"] == "Page");
    CHECK(page["url"] == "/page?x=1");
    CHECK(page["props"]["title"] == "Home");
    CHECK(page["version"] == "v1");
}

TEST_CASE("render answers a first visit with HTML", "[drogon]") {
    Client client;
    const auto reply = client.get("/page");
    CHECK(reply.status == 200);
    CHECK(reply.header("Content-Type").find("text/html") != std::string::npos);
    CHECK(reply.body.find("data-page") != std::string::npos);
    CHECK(reply.header("Vary").find("X-Inertia") != std::string::npos);
}

TEST_CASE("a stale asset version short-circuits with a 409 that is still finalized", "[drogon]") {
    Client client;
    const auto reply = client.send(inertiaGet("/page", "stale"));
    CHECK(reply.status == 409);
    CHECK(reply.header("X-Inertia-Location") ==
          "http://127.0.0.1:" + std::to_string(inertia::drogon_test::port()) + "/page");
    CHECK(reply.header("Vary").find("X-Inertia") != std::string::npos);
}

TEST_CASE("finalize merges into an existing Vary header", "[drogon]") {
    Client client;
    const auto reply = client.get("/vary");
    const auto vary = reply.header("Vary");
    CHECK(vary.find("Accept-Encoding") != std::string::npos);
    CHECK(vary.find("X-Inertia") != std::string::npos);
}

TEST_CASE("location is a 409 for Inertia requests and a 302 otherwise", "[drogon]") {
    Client client;
    const auto inertiaReply = client.send(inertiaGet("/external"));
    CHECK(inertiaReply.status == 409);
    CHECK(inertiaReply.header("X-Inertia-Location") == "https://example.com/away");

    const auto plain = client.get("/external");
    CHECK(plain.status == 302);
    CHECK(plain.header("Location") == "https://example.com/away");
}

TEST_CASE("throwing prop yields 500 and server keeps serving", "[drogon]") {
    Client client;
    const auto failed = client.send(inertiaGet("/throws"));
    CHECK(failed.status == 500);
    CHECK(failed.header("Vary").find("X-Inertia") != std::string::npos);

    const auto after = client.send(inertiaGet("/page"));
    CHECK(after.status == 200);
    CHECK(PageJson::parse(after.body)["props"]["title"] == "Home");
}

TEST_CASE("finalize edits to status and headers reach the wire", "[drogon]") {
    Client client;
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
