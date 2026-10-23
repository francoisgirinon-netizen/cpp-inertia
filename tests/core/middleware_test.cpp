#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>
#include <inertia/session.hpp>

#include <optional>
#include <string>

using namespace inertia;
using namespace inertia::fake;

TEST_CASE("redirect defaults to 302 with Location", "[middleware]") {
    const Response res = redirect("/dashboard");
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location").value_or("") == "/dashboard");
    REQUIRE(res.body.empty());
}

TEST_CASE("redirect accepts an explicit status", "[middleware]") {
    REQUIRE(redirect("/login", 303).status == 303);
}

TEST_CASE("back redirects to the Referer header", "[middleware]") {
    const auto req = FakeRequest::get("/form").with("Referer", "http://localhost/previous");
    const Response res = back(req);
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location").value_or("") == "http://localhost/previous");
}

TEST_CASE("back falls back to / without a Referer", "[middleware]") {
    const Response res = back(FakeRequest::get("/form"));
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location").value_or("") == "/");
}

namespace {

Inertia appWithVersion(std::string version) {
    Config config;
    config.version = [v = std::move(version)] { return v; };
    return Inertia{std::move(config)};
}

} // namespace

TEST_CASE("version mismatch on a GET Inertia request gives 409 with location",
          "[middleware][protocol:version.mismatch]") {
    const auto app = appWithVersion("v2");
    const Response res =
        app.handleVersion(FakeRequest::inertia("/a?b=1", "v1")).value_or(Response{});
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Location").value_or("") == "http://localhost/a?b=1");
    REQUIRE(res.header("X-Inertia-Version").value_or("") == "v2");
    REQUIRE(res.body.empty());
    REQUIRE_FALSE(res.header("X-Inertia").has_value());
}

TEST_CASE("matching version passes through on Inertia requests",
          "[middleware][protocol:version.mismatch]") {
    const auto app = appWithVersion("v2");
    REQUIRE_FALSE(app.handleVersion(FakeRequest::inertia("/a", "v2")).has_value());
}

TEST_CASE("non-Inertia requests never get the version 409",
          "[middleware][protocol:version.mismatch]") {
    const auto app = appWithVersion("v2");
    REQUIRE_FALSE(app.handleVersion(FakeRequest::get("/a")).has_value());
}

TEST_CASE("non-GET requests never get the version 409", "[middleware][protocol:version.non-get]") {
    const auto app = appWithVersion("v2");
    for (const char *verb : {"POST", "PUT", "PATCH", "DELETE"}) {
        auto req = FakeRequest::inertia("/a", "v1");
        req.verb = verb;
        REQUIRE_FALSE(app.handleVersion(req).has_value());
    }
}

TEST_CASE("302 becomes 303 for Inertia PUT, PATCH and DELETE",
          "[middleware][protocol:redirect.303]") {
    const Inertia app{Config{}};
    for (const char *verb : {"PUT", "PATCH", "DELETE"}) {
        auto req = FakeRequest::inertia("/a");
        req.verb = verb;
        Response res = redirect("/done");
        app.finalize(req, res);
        REQUIRE(res.status == 303);
        REQUIRE(res.header("Location").value_or("") == "/done");
    }
}

TEST_CASE("302 stays 302 for Inertia POST", "[middleware][protocol:redirect.303]") {
    const Inertia app{Config{}};
    auto req = FakeRequest::inertia("/a");
    req.verb = "POST";
    Response res = redirect("/done");
    app.finalize(req, res);
    REQUIRE(res.status == 302);
}

TEST_CASE("302 stays 302 for non-Inertia PUT", "[middleware][protocol:redirect.303]") {
    const Inertia app{Config{}};
    auto req = FakeRequest::get("/a");
    req.verb = "PUT";
    Response res = redirect("/done");
    app.finalize(req, res);
    REQUIRE(res.status == 302);
}
