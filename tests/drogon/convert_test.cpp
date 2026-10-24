#include <catch2/catch_test_macros.hpp>

#include <inertia/drogon/drogon.hpp>
#include <inertia/response.hpp>

#include <drogon/Cookie.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <trantor/utils/Date.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

TEST_CASE("DrogonRequest ignores forwarded headers by default", "[drogon]") {
    auto req = makeRequest(::drogon::Get, "/x", "");
    req->addHeader("Host", "internal:8080");
    req->addHeader("X-Forwarded-Proto", "https");
    req->addHeader("X-Forwarded-Host", "public.example");
    const inertia::drogon::DrogonRequest view{req};
    CHECK(view.fullUrl() == "http://internal:8080/x");
    CHECK_FALSE(view.isSecure());
}

TEST_CASE("DrogonRequest uses the first forwarded proto and host when trusted", "[drogon]") {
    auto req = makeRequest(::drogon::Get, "/x", "a=1");
    req->addHeader("Host", "internal:8080");
    req->addHeader("X-Forwarded-Proto", "https, http");
    req->addHeader("X-Forwarded-Host", " public.example , internal:8080");
    const inertia::drogon::DrogonRequest view{req, true};
    CHECK(view.fullUrl() == "https://public.example/x?a=1");
    CHECK(view.isSecure());
}

TEST_CASE("DrogonRequest trusting forwarded headers falls back when they are absent or bad",
          "[drogon]") {
    auto plain = makeRequest(::drogon::Get, "/x", "");
    plain->addHeader("Host", "internal:8080");
    CHECK(inertia::drogon::DrogonRequest{plain, true}.fullUrl() == "http://internal:8080/x");

    auto bad = makeRequest(::drogon::Get, "/x", "");
    bad->addHeader("Host", "internal:8080");
    bad->addHeader("X-Forwarded-Proto", "javascript");
    bad->addHeader("X-Forwarded-Host", "evil.example/path bad");
    const inertia::drogon::DrogonRequest view{bad, true};
    CHECK(view.fullUrl() == "http://internal:8080/x");
    CHECK_FALSE(view.isSecure());

    auto http = makeRequest(::drogon::Get, "/x", "");
    http->addHeader("X-Forwarded-Proto", "HTTP");
    CHECK_FALSE(inertia::drogon::DrogonRequest{http, true}.isSecure());
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

TEST_CASE("toDrogon of an rvalue moves the body into the Drogon response", "[drogon]") {
    inertia::Response res;
    res.status = 200;
    res.body = std::string(4096, 'x');
    res.setHeader("Content-Type", "text/html; charset=utf-8");
    res.setHeader("X-Test", "1");
    const std::string expected = res.body;
    const char *buffer = res.body.data();

    const auto converted = inertia::drogon::toDrogon(std::move(res));
    CHECK(converted->body() == expected);
    CHECK(converted->body().data() == buffer); // the same heap buffer: nothing was copied
    CHECK(converted->statusCode() == ::drogon::k200OK);
    CHECK(converted->getHeader("x-test") == "1");
    CHECK(converted->contentTypeString().starts_with("text/html"));
}

TEST_CASE("toDrogon keeps statuses unknown to Drogon", "[drogon]") {
    inertia::Response res;
    res.status = 419;
    const auto back = inertia::drogon::fromDrogon(inertia::drogon::toDrogon(res));
    CHECK(back.status == 419);
}

TEST_CASE("toDrogon keeps duplicate headers: list headers are comma-joined", "[drogon]") {
    inertia::Response res;
    res.headers.emplace_back("Vary", "X-Inertia");
    res.headers.emplace_back("Content-Type", "text/html");
    res.headers.emplace_back("vary", "Accept-Encoding");
    res.headers.emplace_back("Link", "</a>; rel=preload");
    res.headers.emplace_back("Link", "</b>; rel=preload");
    const auto back = inertia::drogon::fromDrogon(inertia::drogon::toDrogon(res));
    CHECK(back.header("Vary") == "X-Inertia, Accept-Encoding");
    CHECK(back.header("Link") == "</a>; rel=preload, </b>; rel=preload");
}

TEST_CASE("toDrogon does not comma-join single-valued headers: the last one wins", "[drogon]") {
    inertia::Response res;
    res.headers.emplace_back("Location", "/a");
    res.headers.emplace_back("Location", "/b");
    res.headers.emplace_back("Content-Length", "1");
    res.headers.emplace_back("content-length", "2");
    const auto back = inertia::drogon::fromDrogon(inertia::drogon::toDrogon(res));
    CHECK(back.header("Location") == "/b");
    CHECK(back.header("Content-Length") == "2");
}

TEST_CASE("toDrogon keeps every Set-Cookie", "[drogon]") {
    inertia::Response res;
    res.headers.emplace_back("Set-Cookie", "a=1; Path=/");
    res.headers.emplace_back("Set-Cookie", "b=2; Path=/");
    const auto converted = inertia::drogon::toDrogon(res);
    CHECK(converted->getCookie("a").value() == "1");
    CHECK(converted->getCookie("b").value() == "2");
}

TEST_CASE("toDrogon keeps every Set-Cookie attribute", "[drogon]") {
    inertia::Response res;
    res.headers.emplace_back("Set-Cookie",
                             "sid=xyz; Expires=Wed, 21 Oct 2026 07:28:00 GMT; Max-Age=3600; "
                             "Domain=example.com; Path=/app; Secure; HttpOnly; SameSite=Strict");
    const auto converted = inertia::drogon::toDrogon(res);
    const auto cookie = converted->getCookie("sid");
    CHECK(cookie.value() == "xyz");
    CHECK(cookie.getMaxAge() == 3600);
    CHECK(cookie.domain() == "example.com");
    CHECK(cookie.path() == "/app");
    CHECK(cookie.isSecure());
    CHECK(cookie.isHttpOnly());
    CHECK(cookie.sameSite() == ::drogon::Cookie::SameSite::kStrict);
    CHECK(cookie.expiresDate().microSecondsSinceEpoch() == 1792567680LL * 1000000LL);

    const auto line = inertia::drogon::fromDrogon(converted).header("Set-Cookie").value_or("");
    CHECK(line.find("Expires=Wed, 21 Oct 2026 07:28:00 GMT") != std::string::npos);
    CHECK(line.find("Max-Age=3600") != std::string::npos);
    CHECK(line.find("Domain=example.com") != std::string::npos);
    CHECK(line.find("Path=/app") != std::string::npos);
    CHECK(line.find("Secure") != std::string::npos);
    CHECK(line.find("HttpOnly") != std::string::npos);
    CHECK(line.find("SameSite=Strict") != std::string::npos);
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
