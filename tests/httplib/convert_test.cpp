#include <catch2/catch_test_macros.hpp>

#include <inertia/httplib/httplib.hpp>
#include <inertia/response.hpp>

#include <inertia/httplib/detail/httplib_include.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

httplib::Request makeRequest(const std::string &method, const std::string &target) {
    httplib::Request req;
    req.method = method;
    req.target = target;
    req.path = target.substr(0, target.find('?'));
    return req;
}

} // namespace

TEST_CASE("HttplibRequest maps method, url, fullUrl and headers", "[httplib]") {
    auto req = makeRequest("post", "/users?b=%20x&a=1&a=2");
    req.headers.emplace("Host", "example.test:8080");
    req.headers.emplace("X-Inertia", "true");

    const inertia::httplib::HttplibRequest view{req};
    CHECK(view.method() == "POST");
    CHECK(view.url() == "/users?b=%20x&a=1&a=2");
    CHECK(view.fullUrl() == "http://example.test:8080/users?b=%20x&a=1&a=2");
    CHECK(view.header("x-inertia") == "true");
    CHECK(view.header("X-INERTIA") == "true");
    CHECK_FALSE(view.header("X-Missing").has_value());
    CHECK_FALSE(view.isSecure());
}

TEST_CASE("HttplibRequest keeps the raw target, not the decoded path", "[httplib]") {
    auto req = makeRequest("GET", "/a%2Fb%20c?q=%7e&q=%7E");
    req.path = "/a/b c"; // what cpp-httplib decodes
    const inertia::httplib::HttplibRequest view{req};
    CHECK(view.url() == "/a%2Fb%20c?q=%7e&q=%7E");
}

TEST_CASE("HttplibRequest without a raw target rebuilds path and query", "[httplib]") {
    httplib::Request req;
    req.method = "GET";
    req.path = "/search";
    req.params.emplace("q", "a b");
    const inertia::httplib::HttplibRequest view{req};
    CHECK(view.url() == "/search?q=a+b"); // form encoding

    httplib::Request bare;
    CHECK(inertia::httplib::HttplibRequest{bare}.url() == "/");
}

TEST_CASE("HttplibRequest fullUrl defaults the host to localhost", "[httplib]") {
    const auto req = makeRequest("GET", "/x");
    CHECK(inertia::httplib::HttplibRequest{req}.fullUrl() == "http://localhost/x");
}

TEST_CASE("HttplibRequest joins repeated header lines", "[httplib]") {
    auto req = makeRequest("GET", "/");
    req.headers.emplace("X-Multi", "a");
    req.headers.emplace("x-multi", "b");
    CHECK(inertia::httplib::HttplibRequest{req}.header("X-Multi") == "a, b");
}

TEST_CASE("forwarded headers are ignored unless trusted", "[httplib]") {
    auto req = makeRequest("GET", "/p?x=1");
    req.headers.emplace("Host", "internal:8080");
    req.headers.emplace("X-Forwarded-Proto", "https");
    req.headers.emplace("X-Forwarded-Host", "app.example.com");

    const inertia::httplib::HttplibRequest untrusted{req};
    CHECK_FALSE(untrusted.isSecure());
    CHECK(untrusted.fullUrl() == "http://internal:8080/p?x=1");

    const inertia::httplib::HttplibRequest trusted{req, true};
    CHECK(trusted.isSecure());
    CHECK(trusted.fullUrl() == "https://app.example.com/p?x=1");
}

TEST_CASE("trusted forwarded headers use the first value of the list", "[httplib]") {
    auto req = makeRequest("GET", "/");
    req.headers.emplace("X-Forwarded-Proto", "HTTPS, http");
    req.headers.emplace("X-Forwarded-Host", "first.example, second.example");
    const inertia::httplib::HttplibRequest view{req, true};
    CHECK(view.fullUrl() == "https://first.example/");
}

TEST_CASE("a trusted but malformed forwarded host or proto is ignored", "[httplib]") {
    auto req = makeRequest("GET", "/");
    req.headers.emplace("Host", "real.example");
    req.headers.emplace("X-Forwarded-Host", "evil.example/@x");
    req.headers.emplace("X-Forwarded-Proto", "javascript");
    const inertia::httplib::HttplibRequest view{req, true};
    CHECK_FALSE(view.isSecure());
    CHECK(view.fullUrl() == "http://real.example/");
}

TEST_CASE("toHttplib copies status, body and every header including repeated Set-Cookie",
          "[httplib]") {
    inertia::Response res;
    res.status = 419;
    res.body = R"({"a":1})";
    res.headers = {{"Content-Type", "application/json"},
                   {"Set-Cookie", "a=1; Path=/"},
                   {"Set-Cookie", "b=2; Path=/; HttpOnly"},
                   {"Vary", "X-Inertia"},
                   {"Content-Length", "999"}};

    httplib::Response out;
    out.headers.emplace("Stale", "1");
    inertia::httplib::toHttplib(res, out);

    CHECK(out.status == 419);
    CHECK(out.body == res.body);
    CHECK(out.get_header_value("Content-Type") == "application/json");
    CHECK(out.get_header_value("vary") == "X-Inertia");
    CHECK(out.get_header_value_count("Set-Cookie") == 2);
    CHECK(out.get_header_value("Set-Cookie", "", 1) == "b=2; Path=/; HttpOnly");
    CHECK_FALSE(out.has_header("Stale"));
    CHECK_FALSE(out.has_header("Content-Length"));
}

TEST_CASE("fromHttplib reads status, body and headers back", "[httplib]") {
    httplib::Response in;
    in.status = 302;
    in.body = "moved";
    in.set_header("Location", "/next");
    in.headers.emplace("Set-Cookie", "a=1");
    in.headers.emplace("Set-Cookie", "b=2");

    const auto res = inertia::httplib::fromHttplib(in);
    CHECK(res.status == 302);
    CHECK(res.body == "moved");
    CHECK(res.header("location") == "/next");
    CHECK(std::ranges::count_if(res.headers,
                                [](const auto &h) { return h.first == "Set-Cookie"; }) == 2);
}

TEST_CASE("HttplibRequest ignores a Host that cannot be an authority", "[httplib]") {
    for (const char *bad : {"evil host", "a/b", "user@host", "ho st:80", ""}) {
        auto req = makeRequest("GET", "/x");
        req.headers.emplace("Host", bad);
        CHECK(inertia::httplib::HttplibRequest{req}.fullUrl() == "http://localhost/x");
    }
    auto good = makeRequest("GET", "/x");
    good.headers.emplace("Host", "[::1]:8080");
    CHECK(inertia::httplib::HttplibRequest{good}.fullUrl() == "http://[::1]:8080/x");
}

TEST_CASE("HttplibRequest turns an absolute-form target into an origin-relative url", "[httplib]") {
    const auto url = [](const std::string &target) {
        const auto req = makeRequest("GET", target);
        return inertia::httplib::HttplibRequest{req}.url();
    };
    CHECK(url("http://evil.example/x?y=1") == "/x?y=1");
    CHECK(url("HTTPS://evil.example:8443/a/b") == "/a/b");
    CHECK(url("http://evil.example") == "/");
    CHECK(url("http://evil.example?x=1") == "/?x=1");
    CHECK(url("http://evil.example#frag") == "/");
    CHECK(url("/plain?x=1") == "/plain?x=1");
}

TEST_CASE("an asterisk target stays an asterisk and is not glued onto the host", "[httplib]") {
    auto req = makeRequest("OPTIONS", "*");
    req.headers.emplace("Host", "real.test");
    const inertia::httplib::HttplibRequest view{req};
    CHECK(view.url() == "*");
    CHECK(view.fullUrl() == "http://real.test");
}
