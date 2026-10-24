#pragma once

#include <inertia/html.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>
#include <inertia/ssr.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

class Vite;

/// Per-response overrides of the matching Config settings. An unset field keeps the config value.
struct RenderOptions {
    /// Overrides Config::encryptHistory for this response.
    std::optional<bool> encryptHistory;
    /// Overrides Config::preserveBigIntegers for this response.
    std::optional<bool> preserveBigIntegers;
};

/// Settings of an Inertia instance. They are copied into the instance and never change afterwards.
struct Config {
    /// Id of the element the front end mounts on. The page JSON goes in a `<script data-page>` with
    /// the same id.
    std::string rootId = "app";
    /// Returns the asset version sent to the client. When the version differs from the client's,
    /// the next GET Inertia request gets a 409 and the client reloads. Empty means `""`. Use
    /// versionFromVite() to derive it from the Vite manifest.
    std::function<std::string()> version;
    /// Encrypts the client-side history state of every page, unless RenderOptions overrides it.
    bool encryptHistory = false;
    /// Sends `sharedProps`, the top-level keys of the shared props, so the client knows which keys
    /// were shared. Disable it to hide those key names.
    bool exposeSharedPropKeys = true;
    /// Serializes integers outside ±(2^53 − 1) as `{"$bigint": "<digits>"}` and sets the
    /// `preserveBigIntegers` flag, so the client can restore them exactly.
    bool preserveBigIntegers = false;
    /// Reports every validation message of a field as an array, instead of only the first one.
    bool withAllErrors = false;
    /// Builds the HTML document of a first visit from its RootContext.
    RootTemplate rootTemplate = defaultRootTemplate;
    /// Vite integration. When set, first-visit HTML gets its tags in RootContext::viteTags, and
    /// versionFromVite() can derive the asset version from it. Null means no Vite tags.
    std::shared_ptr<const Vite> vite;
    /// Server-side rendering of first visits. When enabled, the SSR server renders the body, and
    /// any failure falls back to client-side rendering.
    SsrConfig ssr;
    /// Clock used for once-prop expiry, for tests. Empty means the system clock.
    std::function<std::chrono::system_clock::time_point()> clock;
};

/// Builds Inertia responses and applies the protocol rules for versions, redirects and headers.
///
/// An instance is configured once at startup with share(), shareWith() and shareOnce(), and is
/// immutable after that. Once setup is complete, the const methods may be called concurrently
/// from request threads without locking, provided each request uses its own RequestView and
/// SessionStore.
///
/// A request is served with page() (the JSON object) or render() (the full response), and the
/// response is then passed through finalize(). handleVersion() and location() cover the cases that
/// do not render a page.
class Inertia {
  public:
    /// Creates an instance from its settings.
    /// @param config Settings, copied into the instance.
    /// @param http HTTP client for SSR requests. Null disables SSR even when `config.ssr` is
    /// enabled.
    explicit Inertia(Config config, std::shared_ptr<HttpClient> http = nullptr);

    /// Shares a prop with every page. Its value is resolved for each response, so a once() or
    /// always() prop keeps its semantics. Call during setup only.
    /// @param key Prop name. A dot path such as `"auth.user"` nests the value.
    /// @param value Prop sent with every page.
    void share(std::string key, Prop value);

    /// Registers a callback that returns props for one request, such as the authenticated user.
    /// Callbacks run in registration order for every page, before the page's own props. Call
    /// during setup only.
    /// @param perRequest Callback that receives the request and its session, and returns props.
    void shareWith(std::function<Props(const RequestView &, SessionStore &)> perRequest);

    /// Shares a once prop: the client keeps the value and the server stops resending it until the
    /// client drops it. Same as `share(key, once(fn))`. Call during setup only.
    /// @param key Prop name.
    /// @param fn Callable that produces the value.
    void shareOnce(std::string key, Resolver fn);

    /// @return The asset version sent to the client, or `""` when Config::version is empty.
    [[nodiscard]] std::string version() const;

    /// Builds the page object of an Inertia response: `component`, `props` and the metadata, as
    /// described by the protocol. It does not produce an HTTP response, so it suits JSON-only
    /// endpoints and tests.
    ///
    /// The one-shot session entries (validation errors, flash data and the history flags) are
    /// consumed by this call, so build a page once per request.
    /// @param req Request being served.
    /// @param session Session of the request. One-shot entries are pulled from it.
    /// @param component Name of the front-end component to render.
    /// @param props Page props. They are copied, so the caller keeps them.
    /// @param opts Per-response overrides of the config.
    /// @return The page object.
    [[nodiscard]] Json page(const RequestView &req, SessionStore &session,
                            std::string_view component, const Props &props,
                            const RenderOptions &opts = {}) const;

    /// Builds the complete response for a page: the JSON page for an Inertia request, or the HTML
    /// document for a first visit. Props and the component are consumed.
    ///
    /// `req` and `session` are read only before this call returns. `done` receives the response
    /// exactly once, and it may run after render() has returned: SSR completes asynchronously,
    /// and then `done` runs on the thread that delivers the SSR reply. The response is not
    /// finalized, so pass it through finalize() before it is sent.
    /// @param req Request being served.
    /// @param session Session of the request. One-shot entries are pulled from it.
    /// @param component Name of the front-end component to render.
    /// @param props Page props, consumed by the call.
    /// @param done Receives the finished response.
    /// @param opts Per-response overrides of the config.
    void render(const RequestView &req, SessionStore &session, std::string component, Props props,
                std::function<void(Response)> done, RenderOptions opts = {}) const;

    /// Pre-routing check for stale assets. A GET Inertia request whose `X-Inertia-Version` differs
    /// from version() gets a 409 with `X-Inertia-Location` set to the full request URL and
    /// `X-Inertia-Version` set to the current version. The client then does a full page load.
    /// Call it before routing.
    /// @param req Request being served.
    /// @return The 409 response, or nullopt when the request may proceed.
    [[nodiscard]] std::optional<Response> handleVersion(const RequestView &req) const;

    /// Redirects to a URL outside the Inertia app. An Inertia request gets a 409 with
    /// `X-Inertia-Location`, so the client performs a full navigation. Any other request gets a
    /// 302 to `url`.
    /// @param req Request being served.
    /// @param url Absolute URL to redirect to.
    /// @return The redirect response.
    [[nodiscard]] Response location(const RequestView &req, std::string_view url) const;

    /// Applies the request-dependent protocol rules to a response, in place. Call it on every
    /// response before sending it. It does the following:
    /// - For an Inertia request, a 302 becomes 303 on PUT, PATCH and DELETE.
    /// - For an Inertia request, an empty 200 becomes a 302 to `Referer`, or to `/` when there is
    ///   none.
    /// - For an Inertia request that is not a prefetch, a redirect whose `Location` contains a
    ///   fragment becomes a 409 with `X-Inertia-Redirect`, because the client cannot follow it.
    /// - Adds `X-Inertia` to `Vary`, keeping the existing values and removing duplicates.
    /// @param req Request that produced the response.
    /// @param res Response to adjust.
    void finalize(const RequestView &req, Response &res) const;

  private:
    /// Builds the page object shared by page() and render().
    [[nodiscard]] Json buildPage(const RequestView &req, SessionStore &session,
                                 std::string component, Props props,
                                 const RenderOptions &opts) const;

    Config config_;
    SsrGateway ssr_;
    Props shared_;
    std::vector<std::function<Props(const RequestView &, SessionStore &)>> perRequest_;
};

} // namespace inertia
