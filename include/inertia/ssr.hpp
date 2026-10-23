#pragma once

#include <inertia/json.hpp>
#include <inertia/ports/http_client.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace inertia {

class Vite;

struct SsrConfig {
    bool enabled = false;
    std::string url = "http://127.0.0.1:13714";
    std::chrono::milliseconds timeout{1500};
    // Request paths that are never server-rendered: an exact path or a prefix ending in "*".
    std::vector<std::string> exceptPaths;
};

struct SsrResult {
    std::string head; // head elements joined with "\n"
    std::string body;
};

// Asks an Inertia SSR server to render a page. Every failure yields nullopt; nothing throws.
class SsrGateway {
  public:
    SsrGateway(SsrConfig cfg, std::shared_ptr<HttpClient> http, std::shared_ptr<const Vite> vite);

    // True when SSR is enabled, a client is available and the path is not excluded.
    [[nodiscard]] bool shouldRender(std::string_view requestPath) const;

    void render(const Json &page, std::string_view requestPath,
                std::function<void(std::optional<SsrResult>)> done) const;

    // GET <url>/health; true only for {"status":"OK"}.
    void health(std::function<void(bool)> done) const;

  private:
    [[nodiscard]] std::string baseUrl() const;
    [[nodiscard]] std::string renderUrl() const;

    SsrConfig cfg_;
    std::shared_ptr<HttpClient> http_;
    std::shared_ptr<const Vite> vite_;
};

} // namespace inertia
