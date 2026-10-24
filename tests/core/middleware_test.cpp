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

TEST_CASE("Inertia redirect with a fragment becomes 409 with X-Inertia-Redirect",
          "[middleware][protocol:redirect.fragment]") {
    const Inertia app{Config{}};
    auto req = FakeRequest::inertia("/a");
    req.verb = "POST";
    Response res = redirect("/a#b");
    app.finalize(req, res);
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Redirect").value_or("") == "/a#b");
    REQUIRE(res.body.empty());
    REQUIRE_FALSE(res.header("Location").has_value());
    REQUIRE_FALSE(res.header("X-Inertia").has_value());
}

TEST_CASE("fragment redirect becomes 409 for any method",
          "[middleware][protocol:redirect.fragment]") {
    const Inertia app{Config{}};
    auto req = FakeRequest::inertia("/a");
    req.verb = "GET";
    Response res = redirect("/a#b", 303);
    app.finalize(req, res);
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Redirect").value_or("") == "/a#b");
}

TEST_CASE("prefetch redirect with a fragment keeps its status",
          "[middleware][protocol:redirect.fragment]") {
    const Inertia app{Config{}};
    auto req = FakeRequest::inertia("/a").with("Purpose", "prefetch");
    Response res = redirect("/a#b");
    app.finalize(req, res);
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location").value_or("") == "/a#b");
}

TEST_CASE("non-Inertia redirect with a fragment keeps its status",
          "[middleware][protocol:redirect.fragment]") {
    const Inertia app{Config{}};
    Response res = redirect("/a#b");
    app.finalize(FakeRequest::get("/a"), res);
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location").value_or("") == "/a#b");
}

TEST_CASE("location on an Inertia request gives 409 with X-Inertia-Location",
          "[middleware][protocol:redirect.external]") {
    const Inertia app{Config{}};
    const Response res = app.location(FakeRequest::inertia("/a"), "https://x.test");
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Location").value_or("") == "https://x.test");
    REQUIRE(res.body.empty());
    REQUIRE_FALSE(res.header("X-Inertia").has_value());
}

TEST_CASE("location on a non-Inertia request gives 302 with Location",
          "[middleware][protocol:redirect.external]") {
    const Inertia app{Config{}};
    const Response res = app.location(FakeRequest::get("/a"), "https://x.test");
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location").value_or("") == "https://x.test");
}

TEST_CASE("empty Inertia 200 redirects back to the Referer",
          "[middleware][protocol:response.empty]") {
    const Inertia app{Config{}};
    auto req = FakeRequest::inertia("/a").with("Referer", "http://localhost/prev");
    Response res;
    app.finalize(req, res);
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location").value_or("") == "http://localhost/prev");
}

TEST_CASE("empty Inertia 200 redirects to / without a Referer",
          "[middleware][protocol:response.empty]") {
    const Inertia app{Config{}};
    Response res;
    app.finalize(FakeRequest::inertia("/a"), res);
    REQUIRE(res.status == 302);
    REQUIRE(res.header("Location").value_or("") == "/");
}

TEST_CASE("empty Inertia response redirect is 303 for PUT",
          "[middleware][protocol:response.empty]") {
    const Inertia app{Config{}};
    auto req = FakeRequest::inertia("/a");
    req.verb = "PUT";
    Response res;
    app.finalize(req, res);
    REQUIRE(res.status == 303);
    REQUIRE(res.header("Location").value_or("") == "/");
}

TEST_CASE("body \"0\" is not an empty response", "[middleware][protocol:response.empty]") {
    const Inertia app{Config{}};
    Response res;
    res.body = "0";
    app.finalize(FakeRequest::inertia("/a"), res);
    REQUIRE(res.status == 200);
    REQUIRE_FALSE(res.header("Location").has_value());
}

TEST_CASE("empty non-Inertia 200 is left alone", "[middleware]") {
    const Inertia app{Config{}};
    Response res;
    app.finalize(FakeRequest::get("/a"), res);
    REQUIRE(res.status == 200);
}

TEST_CASE("finalize appends X-Inertia to an existing Vary", "[middleware][protocol:vary.always]") {
    const Inertia app{Config{}};
    Response res;
    res.body = "x";
    res.setHeader("Vary", "Accept-Encoding");
    app.finalize(FakeRequest::get("/a"), res);
    REQUIRE(res.header("Vary").value_or("") == "Accept-Encoding, X-Inertia");
}

TEST_CASE("finalize deduplicates Vary tokens case-insensitively",
          "[middleware][protocol:vary.always]") {
    const Inertia app{Config{}};
    Response res;
    res.body = "x";
    res.setHeader("Vary", "accept-encoding, Accept-Encoding, x-inertia, ACCEPT-ENCODING");
    app.finalize(FakeRequest::get("/a"), res);
    REQUIRE(res.header("Vary").value_or("") == "accept-encoding, x-inertia");
}

TEST_CASE("finalize leaves Vary: * untouched", "[middleware][protocol:vary.always]") {
    const Inertia app{Config{}};
    Response res;
    res.body = "x";
    res.setHeader("Vary", "*");
    app.finalize(FakeRequest::get("/a"), res);
    REQUIRE(res.header("Vary").value_or("") == "*");
}

TEST_CASE("a GET Inertia request without X-Inertia-Version gets the version 409",
          "[middleware][protocol:version.mismatch]") {
    const auto app = appWithVersion("v2");
    const auto req = FakeRequest::get("/a?b=1").with("X-Inertia", "true");
    const Response res = app.handleVersion(req).value_or(Response{});
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Location").value_or("") == "http://localhost/a?b=1");
    REQUIRE(res.header("X-Inertia-Version").value_or("") == "v2");
}

TEST_CASE("fragment redirects with 301 and 307 become 409",
          "[middleware][protocol:redirect.fragment]") {
    const Inertia app{Config{}};
    for (const int status : {301, 307}) {
        Response res = redirect("/a#b", status);
        app.finalize(FakeRequest::inertia("/a"), res);
        REQUIRE(res.status == 409);
        REQUIRE(res.header("X-Inertia-Redirect").value_or("") == "/a#b");
        REQUIRE_FALSE(res.header("Location").has_value());
    }
}

TEST_CASE("a version 409 keeps its status through finalize and gains Vary",
          "[middleware][protocol:vary.always]") {
    const auto app = appWithVersion("v2");
    const auto req = FakeRequest::inertia("/a", "v1");
    Response res = app.handleVersion(req).value_or(Response{});
    app.finalize(req, res);
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Location").has_value());
    REQUIRE(res.header("Vary").value_or("") == "X-Inertia");
}

TEST_CASE("a location 409 keeps its status through finalize and gains Vary",
          "[middleware][protocol:vary.always]") {
    const Inertia app{Config{}};
    const auto req = FakeRequest::inertia("/a");
    Response res = app.location(req, "https://x.test");
    app.finalize(req, res);
    REQUIRE(res.status == 409);
    REQUIRE(res.header("X-Inertia-Location").value_or("") == "https://x.test");
    REQUIRE(res.header("Vary").value_or("") == "X-Inertia");
}
