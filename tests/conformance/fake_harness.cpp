#include "harness.hpp"
#include "routes.hpp"

#include <inertia/fake/fake.hpp>
#include <inertia/headers.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia::conformance {

std::optional<std::string> TestResponse::header(std::string_view name) const {
    for (const auto &[key, value] : headers) {
        if (headers::iequals(key, name)) {
            return value;
        }
    }
    return std::nullopt;
}

Json TestResponse::json() const { return Json::parse(body); }

namespace {

constexpr std::string_view kResetPath = "/__reset";

// One application and one session per test process; POST /__reset replaces the session.
struct World {
    Inertia app = makeConformanceApp();
    std::vector<Route> routes = conformanceRoutes();
    fake::MemorySession session;
};

World &world() {
    static World instance;
    return instance;
}

const Route *findRoute(const World &w, std::string_view method, std::string_view path) {
    const auto it = std::ranges::find_if(
        w.routes, [&](const Route &route) { return route.method == method && route.path == path; });
    return it == w.routes.end() ? nullptr : &*it;
}

TestResponse toTestResponse(Response res) {
    return {.status = res.status, .headers = std::move(res.headers), .body = std::move(res.body)};
}

} // namespace

TestResponse send(const TestRequest &req) {
    auto &w = world();
    if (req.method == "POST" && req.url == kResetPath) {
        w.session = fake::MemorySession{};
        return {.status = 204, .headers = {}, .body = {}};
    }

    fake::FakeRequest fakeReq;
    fakeReq.verb = req.method;
    fakeReq.path = req.url;
    for (const auto &[name, value] : req.headers) {
        fakeReq.with(name, value);
    }

    Response res;
    if (auto early = w.app.handleVersion(fakeReq)) {
        res = std::move(*early);
    } else {
        const auto path = std::string_view{req.url}.substr(0, req.url.find('?'));
        const Route *route = findRoute(w, req.method, path);
        if (route == nullptr) {
            res.status = 404;
        } else {
            bool called = false;
            route->handler(fakeReq, w.session, w.app, [&](Response r) {
                res = std::move(r);
                called = true;
            });
            if (!called) {
                throw std::logic_error("route did not complete synchronously: " + req.url);
            }
        }
    }
    w.app.finalize(fakeReq, res);
    return toTestResponse(std::move(res));
}

} // namespace inertia::conformance
