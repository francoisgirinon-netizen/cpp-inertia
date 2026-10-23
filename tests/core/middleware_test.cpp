#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
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
