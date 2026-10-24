// A complete adapter written the way a consumer of the library would write it: only the installed
// public headers, no inertia::fake. It is an in-process "framework" (a request struct, a session
// map and a dispatcher), enough to implement the RequestView and SessionStore ports and the
// conformance kit's send(); the kit's scenarios (inertia::conformance) then run against it.
#include <inertia/conformance/harness.hpp>
#include <inertia/conformance/routes.hpp>
#include <inertia/headers.hpp>
#include <inertia/inertia.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// RequestView over the application's own request type.
class CustomRequest final : public inertia::RequestView {
  public:
    explicit CustomRequest(const inertia::conformance::TestRequest &raw) : raw_(raw) {}

    [[nodiscard]] std::string_view method() const override { return raw_.method; }
    [[nodiscard]] std::string url() const override { return raw_.url; }
    [[nodiscard]] std::string fullUrl() const override { return "http://custom.test" + raw_.url; }
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override {
        for (const auto &[key, value] : raw_.headers) {
            if (inertia::headers::iequals(key, name)) {
                return value;
            }
        }
        return std::nullopt;
    }

  private:
    const inertia::conformance::TestRequest &raw_;
};

// SessionStore over the application's own session container.
class CustomSession final : public inertia::SessionStore {
  public:
    [[nodiscard]] std::optional<inertia::Json> get(std::string_view key) const override {
        const auto it = data_.find(key);
        return it == data_.end() ? std::nullopt : std::optional{it->second};
    }
    void put(std::string key, inertia::Json value) override {
        data_.insert_or_assign(std::move(key), std::move(value));
    }
    std::optional<inertia::Json> pull(std::string_view key) override {
        const auto it = data_.find(key);
        if (it == data_.end()) {
            return std::nullopt;
        }
        auto value = std::move(it->second);
        data_.erase(it);
        return value;
    }

  private:
    std::map<std::string, inertia::Json, std::less<>> data_;
};

// One application and one session for the whole test process.
struct World {
    inertia::Inertia app = inertia::conformance::makeConformanceApp();
    std::vector<inertia::conformance::Route> routes = inertia::conformance::conformanceRoutes();
    CustomSession session;
};

World &world() {
    static World instance;
    return instance;
}

} // namespace

namespace inertia::conformance {

TestResponse send(const TestRequest &req) {
    auto &w = world();
    if (req.method == "POST" && req.url == "/__reset") {
        w.session = CustomSession{};
        return {.status = 204, .headers = {}, .body = {}};
    }

    const CustomRequest request{req};
    Response res;
    // The pipeline: version check, then the route, then finalize.
    if (auto early = w.app.handleVersion(request)) {
        res = std::move(*early);
    } else {
        const std::string_view path = std::string_view{req.url}.substr(0, req.url.find('?'));
        const auto route = std::ranges::find_if(w.routes, [&](const Route &candidate) {
            return candidate.method == req.method && candidate.path == path;
        });
        if (route == w.routes.end()) {
            res.status = 404;
        } else {
            bool answered = false;
            route->handler(request, w.session, w.app, [&](Response reply) {
                res = std::move(reply);
                answered = true;
            });
            if (!answered) {
                throw std::logic_error("route did not complete synchronously: " + req.url);
            }
        }
    }
    w.app.finalize(request, res);
    return {.status = res.status, .headers = std::move(res.headers), .body = std::move(res.body)};
}

} // namespace inertia::conformance
