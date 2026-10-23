#include <catch2/catch_test_macros.hpp>

#include <inertia/drogon/drogon.hpp>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <string>

namespace {

::drogon::HttpRequestPtr makeRequest(::drogon::HttpMethod method, const std::string &path,
                                     const std::string &query) {
    auto req = ::drogon::HttpRequest::newHttpRequest();
    req->setMethod(method);
    req->setPath(path);
    req->setPathEncode(false);
    if (!query.empty()) {
        req->setPath(path + "?" + query);
    }
    return req;
}

} // namespace

TEST_CASE("DrogonRequest maps method, url, fullUrl and headers", "[drogon]") {
    auto req = ::drogon::HttpRequest::newHttpRequest();
    req->setMethod(::drogon::Post);
    req->setPathEncode(false);
    req->setPath("/users?b=%20x&a=1&a=2");
    req->addHeader("Host", "example.test:8080");
    req->addHeader("X-Inertia", "true");

    const inertia::drogon::DrogonRequest view{req};
    CHECK(view.method() == "POST");
    CHECK(view.url() == "/users?b=%20x&a=1&a=2");
    CHECK(view.fullUrl() == "http://example.test:8080/users?b=%20x&a=1&a=2");
    CHECK(view.header("x-inertia") == "true");
    CHECK(view.header("X-INERTIA") == "true");
    CHECK_FALSE(view.header("X-Missing").has_value());
}

TEST_CASE("DrogonRequest without a query has a bare path", "[drogon]") {
    const inertia::drogon::DrogonRequest view{makeRequest(::drogon::Get, "/plain", "")};
    CHECK(view.method() == "GET");
    CHECK(view.url() == "/plain");
}

TEST_CASE("toDrogon and fromDrogon round-trip status, headers and body", "[drogon]") {
    inertia::Response res;
    res.status = 409;
    res.body = R"({"a":1})";
    res.setHeader("Content-Type", "application/json");
    res.setHeader("X-Inertia-Location", "https://example.com/away");
    res.setHeader("Vary", "X-Inertia");

    const auto converted = inertia::drogon::toDrogon(res);
    REQUIRE(converted->statusCode() == static_cast<::drogon::HttpStatusCode>(409));
    REQUIRE(converted->body() == res.body);

    const auto back = inertia::drogon::fromDrogon(converted);
    CHECK(back.status == 409);
    CHECK(back.body == res.body);
    CHECK(back.header("x-inertia-location") == "https://example.com/away");
    CHECK(back.header("vary") == "X-Inertia");
    CHECK(back.header("content-type").value_or("").starts_with("application/json"));
}

TEST_CASE("toDrogon keeps statuses unknown to Drogon", "[drogon]") {
    inertia::Response res;
    res.status = 419;
    const auto back = inertia::drogon::fromDrogon(inertia::drogon::toDrogon(res));
    CHECK(back.status == 419);
}

TEST_CASE("toDrogon converts Set-Cookie headers to cookies and back", "[drogon]") {
    inertia::Response res;
    res.headers.emplace_back("Set-Cookie", "XSRF-TOKEN=abc123; Path=/; SameSite=Lax");
    const auto converted = inertia::drogon::toDrogon(res);
    REQUIRE(converted->cookies().contains("XSRF-TOKEN"));
    CHECK(converted->getCookie("XSRF-TOKEN").value() == "abc123");

    const auto back = inertia::drogon::fromDrogon(converted);
    CHECK(back.header("Set-Cookie").value_or("").find("XSRF-TOKEN=abc123") != std::string::npos);
}
