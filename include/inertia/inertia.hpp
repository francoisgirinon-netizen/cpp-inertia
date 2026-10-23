#pragma once

#include <inertia/html.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/response.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

// Temporary: replaced by the real Prop type.
using Props = std::vector<std::pair<std::string, Json>>;

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
};

class Inertia {
  public:
    explicit Inertia(Config config, std::shared_ptr<HttpClient> http = nullptr);

    [[nodiscard]] std::string version() const;

    [[nodiscard]] Json page(const RequestView &req, SessionStore &session,
                            std::string_view component, const Props &props,
                            const RenderOptions &opts = {}) const;

  private:
    Config config_;
    std::shared_ptr<HttpClient> http_;
};

} // namespace inertia
