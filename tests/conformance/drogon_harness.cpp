// Runs the shared conformance scenarios over real HTTP against a Drogon application: the routes of
// conformanceRoutes() are registered as Drogon handlers that go through the Drogon adapter.
#include "harness.hpp"
#include "routes.hpp"
#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>

#include <drogon/HttpAppFramework.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace inertia::drogon_test {

namespace {

::drogon::HttpMethod methodOf(const std::string &verb) {
    if (verb == "POST") {
        return ::drogon::Post;
    }
    if (verb == "PUT") {
        return ::drogon::Put;
    }
    if (verb == "PATCH") {
        return ::drogon::Patch;
    }
    if (verb == "DELETE") {
        return ::drogon::Delete;
    }
    return ::drogon::Get;
}

} // namespace

void configureApp() {
    auto &app = ::drogon::app();
    app.enableSession(std::chrono::minutes(10));

    auto inertia = std::make_shared<const Inertia>(conformance::makeConformanceApp());
    // The scenarios send no CSRF tokens; CSRF has its own adapter tests.
    inertia::drogon::CsrfOptions csrf;
    csrf.enabled = false;
    inertia::drogon::install(inertia, std::move(csrf));

    for (auto &route : conformance::conformanceRoutes()) {
        auto handler = std::make_shared<conformance::Handler>(std::move(route.handler));
        app.registerHandler(
            route.path,
            [handler, inertia](const ::drogon::HttpRequestPtr &req,
                               std::function<void(const ::drogon::HttpResponsePtr &)> &&done) {
                auto view = std::make_shared<inertia::drogon::DrogonRequest>(req);
                auto session = std::make_shared<inertia::drogon::DrogonSession>(req->session());
                auto reply =
                    std::make_shared<std::function<void(const ::drogon::HttpResponsePtr &)>>(
                        std::move(done));
                (*handler)(*view, *session, *inertia, [view, session, reply](const Response &res) {
                    (*reply)(inertia::drogon::toDrogon(res));
                });
            },
            {methodOf(route.method)});
    }
}

} // namespace inertia::drogon_test

namespace inertia::conformance {

namespace {
constexpr std::string_view kResetPath = "/__reset";
} // namespace

TestResponse send(const TestRequest &req) {
    // One client (hence one cookie jar, hence one session) per process.
    static drogon_test::Client client;
    if (req.method == "POST" && req.url == kResetPath) {
        client.resetCookies();
        return {.status = 204, .headers = {}, .body = {}};
    }
    drogon_test::Request request;
    request.method = req.method;
    request.url = req.url;
    request.headers = req.headers;
    request.body = req.body;
    auto reply = client.send(request);
    return {
        .status = reply.status, .headers = std::move(reply.headers), .body = std::move(reply.body)};
}

} // namespace inertia::conformance
