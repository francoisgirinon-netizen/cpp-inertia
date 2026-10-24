// Runs the shared conformance scenarios over real HTTP against a cpp-httplib application: the
// routes of conformanceRoutes() are registered as cpp-httplib handlers that go through the httplib
// adapter.
#include "test_support.hpp"

#include <inertia/conformance/harness.hpp>
#include <inertia/conformance/routes.hpp>
#include <inertia/httplib/httplib.hpp>
#include <inertia/inertia.hpp>
#include <inertia/response.hpp>

#include <httplib.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia::conformance {

namespace {

constexpr std::string_view kResetPath = "/__reset";

// One server (hence one session store) and one client (hence one cookie jar, hence one session)
// per test process; POST /__reset empties the jar.
class World {
  public:
    World() : client_(boot()) {}

    httplib_test::Client &client() { return client_; }

  private:
    // Registers the routes and starts the server; returns its port.
    std::uint16_t boot() {
        const auto app = std::make_shared<const Inertia>(makeConformanceApp());
        // The scenarios send no CSRF tokens; CSRF has its own adapter tests.
        inertia::httplib::Options options;
        options.csrf.enabled = false;
        auto &raw = server_.server();
        inertia::httplib::install(raw, app, std::move(options));

        for (auto &route : conformanceRoutes()) {
            const auto handler = std::make_shared<Handler>(std::move(route.handler));
            const auto serve = [handler, app](const ::httplib::Request &req,
                                              ::httplib::Response &res) {
                const inertia::httplib::HttplibRequest view{req};
                auto session = inertia::httplib::session(res);
                bool answered = false;
                (*handler)(view, session, *app, [&res, &answered](const Response &reply) {
                    inertia::httplib::toHttplib(reply, res);
                    answered = true;
                });
                if (!answered) {
                    throw std::logic_error("route did not complete synchronously: " + req.target);
                }
            };
            if (route.method == "GET") {
                raw.Get(route.path, serve);
            } else if (route.method == "POST") {
                raw.Post(route.path, serve);
            } else if (route.method == "PUT") {
                raw.Put(route.path, serve);
            } else if (route.method == "PATCH") {
                raw.Patch(route.path, serve);
            } else if (route.method == "DELETE") {
                raw.Delete(route.path, serve);
            } else {
                throw std::logic_error("unsupported route method: " + route.method);
            }
        }
        server_.start();
        return server_.port();
    }

    httplib_test::TestServer server_;
    httplib_test::Client client_; // declared after server_: boot() runs from its initializer
};

World &world() {
    static World instance;
    return instance;
}

} // namespace

TestResponse send(const TestRequest &req) {
    auto &w = world();
    if (req.method == "POST" && req.url == kResetPath) {
        w.client().resetCookies();
        return {.status = 204, .headers = {}, .body = {}};
    }
    httplib_test::Request request;
    request.method = req.method;
    request.url = req.url;
    request.headers = req.headers;
    request.body = req.body;
    auto reply = w.client().send(request);
    return {
        .status = reply.status, .headers = std::move(reply.headers), .body = std::move(reply.body)};
}

} // namespace inertia::conformance
