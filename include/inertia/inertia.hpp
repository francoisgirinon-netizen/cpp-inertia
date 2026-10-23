#pragma once

#include <inertia/html.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

struct RenderOptions {
    std::optional<bool> encryptHistory;
    std::optional<bool> preserveBigIntegers;
};

struct Config {
    std::string rootId = "app";
    std::function<std::string()> version; // empty -> ""
    bool encryptHistory = false;
    bool exposeSharedPropKeys = true;
    bool preserveBigIntegers = false;
    bool withAllErrors = false;
    RootTemplate rootTemplate = defaultRootTemplate;
    // Test hook: pins the time used for once-prop expiry. Empty -> system clock.
    std::function<std::chrono::system_clock::time_point()> clock;
};

class Inertia {
  public:
    explicit Inertia(Config config, std::shared_ptr<HttpClient> http = nullptr);

    // Registration is for startup only: Inertia is immutable once it serves requests, so
    // concurrent page() calls need no locking.
    void share(std::string key, Prop value);
    void shareWith(std::function<Props(const RequestView &, SessionStore &)> perRequest);
    void shareOnce(std::string key, Resolver fn); // == share(key, once(fn))

    [[nodiscard]] std::string version() const;

    [[nodiscard]] Json page(const RequestView &req, SessionStore &session,
                            std::string_view component, const Props &props,
                            const RenderOptions &opts = {}) const;

    void render(const RequestView &req, SessionStore &session, std::string component, Props props,
                std::function<void(Response)> done, RenderOptions opts = {}) const;

    // Pre-route check: a GET Inertia request whose X-Inertia-Version differs from the current
    // version gets a 409 that asks the client to reload. Any other request yields nullopt.
    [[nodiscard]] std::optional<Response> handleVersion(const RequestView &req) const;

    // Adds Vary: X-Inertia (merged with any existing value, deduplicated).
    void finalize(const RequestView &req, Response &res) const;

  private:
    Config config_;
    std::shared_ptr<HttpClient> http_;
    Props shared_;
    std::vector<std::function<Props(const RequestView &, SessionStore &)>> perRequest_;
};

} // namespace inertia
