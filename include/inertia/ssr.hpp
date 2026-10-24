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

/// Server-side rendering (SSR) settings. The SSR server is a separate Node process that renders a
/// page to HTML for first visits.
struct SsrConfig {
    /// Turns server-side rendering of first visits on or off.
    bool enabled = false;
    /// Base URL of the SSR server. Pages are posted to `<url>/render`. While a Vite hot file
    /// exists, the dev server is used instead.
    std::string url = "http://127.0.0.1:13714";
    /// Maximum wait for the SSR server. On timeout the page falls back to client-side rendering.
    std::chrono::milliseconds timeout{1500};
    /// Request paths that are never server-rendered. An entry is an exact path, or a prefix when
    /// it ends in `*`, such as `"/admin/*"`.
    std::vector<std::string> exceptPaths;
};

/// Markup returned by the SSR server.
struct SsrResult {
    /// Elements for the document head, joined with `\n`.
    std::string head;
    /// Markup that replaces the page script and the root element.
    std::string body;
};

/// Asks the Inertia SSR server to render a page.
///
/// No operation throws on failure. A failed render reports nullopt, and the caller falls back to
/// client-side rendering. Requests are asynchronous: each callback runs exactly once, when the
/// reply arrives or the HTTP client fails, and possibly on another thread.
class SsrGateway {
  public:
    /// Creates a gateway.
    /// @param cfg SSR settings.
    /// @param http HTTP client for SSR requests. Null means SSR never renders.
    /// @param vite Vite integration. Its hot file, when present, redirects renders to the dev
    /// server.
    SsrGateway(SsrConfig cfg, std::shared_ptr<HttpClient> http, std::shared_ptr<const Vite> vite);

    /// Decides whether a request is eligible for server-side rendering.
    /// @param requestPath Request path, without the query string.
    /// @return `true` when SSR is enabled, an HTTP client is available and the path is not in
    /// `exceptPaths`.
    [[nodiscard]] bool shouldRender(std::string_view requestPath) const;

    /// Renders a page on the SSR server. The caller decides first whether the request path
    /// should be rendered, with shouldRender().
    /// @param page Page object from Inertia::page(), posted as JSON.
    /// @param done Receives the rendered markup, or nullopt on any failure: connection error,
    /// timeout, non-2xx status or a malformed reply.
    void render(const Json &page, std::function<void(std::optional<SsrResult>)> done) const;

    /// Checks the SSR server's `GET /health` endpoint.
    /// @param done Receives `true` only when the reply is `{"status": "OK"}`.
    void health(std::function<void(bool)> done) const;

  private:
    /// The configured URL without trailing slashes.
    [[nodiscard]] std::string baseUrl() const;
    /// The URL that render() posts to: the dev server's SSR path when the hot file exists,
    /// otherwise `<url>/render`.
    [[nodiscard]] std::string renderUrl() const;

    SsrConfig cfg_;
    std::shared_ptr<HttpClient> http_;
    std::shared_ptr<const Vite> vite_;
};

} // namespace inertia
