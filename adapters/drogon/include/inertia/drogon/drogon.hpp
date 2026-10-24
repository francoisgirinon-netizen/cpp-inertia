#pragma once

#include <inertia/inertia.hpp>

#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/Session.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Inside this namespace the global Drogon namespace must always be spelled ::drogon.
namespace inertia::drogon {

// Read-only view of a Drogon request. Not thread safe, like the request it wraps.
class DrogonRequest final : public RequestView {
  public:
    // trustForwardedHeaders: take the scheme and host from the first value of X-Forwarded-Proto /
    // X-Forwarded-Host. Only for servers behind a proxy that sets (and overwrites) them.
    explicit DrogonRequest(::drogon::HttpRequestPtr req, bool trustForwardedHeaders = false);

    [[nodiscard]] std::string_view method() const override;
    [[nodiscard]] std::string url() const override;
    [[nodiscard]] std::string fullUrl() const override;
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override;
    // https on the connection or, when forwarded headers are trusted, per X-Forwarded-Proto.
    [[nodiscard]] bool isSecure() const;

  private:
    ::drogon::HttpRequestPtr req_;
    bool trustForwarded_;
};

[[nodiscard]] ::drogon::HttpResponsePtr toDrogon(const Response &res);
[[nodiscard]] Response fromDrogon(const ::drogon::HttpResponsePtr &res);

// Stores each value as its dumped JSON text inside the Drogon session. Drogon's Session has its
// own lock, so a store is safe to use while other requests of the same session run.
class DrogonSession final : public SessionStore {
  public:
    // Throws std::invalid_argument for a null session (sessions not enabled).
    explicit DrogonSession(::drogon::SessionPtr session);

    [[nodiscard]] std::optional<Json> get(std::string_view key) const override;
    void put(std::string key, Json value) override;
    std::optional<Json> pull(std::string_view key) override;

  private:
    ::drogon::SessionPtr session_;
};

// The SSR transport: one ::drogon::HttpClient per origin, created on first use and shared by every
// request. Callbacks run on the client's event-loop thread.
// Drogon cannot cancel a request and does not pipeline, so a hung SSR server would stall every
// later request queued on the same connection. After a timeout the origin's client is therefore
// dropped from the pool (at most once per timeout window) and the next request opens a fresh
// connection; the old client lives on until Drogon is done with its pending request.
class DrogonHttpClient final : public HttpClient {
  public:
    // nullptr: use the application's event loop.
    explicit DrogonHttpClient(trantor::EventLoop *loop = nullptr);

    void post(std::string url, std::string jsonBody, std::chrono::milliseconds timeout,
              std::function<void(HttpResult)> done) override;
    void get(std::string url, std::chrono::milliseconds timeout,
             std::function<void(HttpResult)> done) override;

  private:
    void send(::drogon::HttpMethod method, std::string_view url, std::string body,
              std::chrono::milliseconds timeout, std::function<void(HttpResult)> done);

    // Shared with the in-flight callbacks, which may outlive this object.
    struct Pool;

    trantor::EventLoop *loop_;
    std::shared_ptr<Pool> pool_;
};

struct CsrfOptions {
    bool enabled = true;
    std::string cookie = "XSRF-TOKEN"; // readable by scripts (not HttpOnly), as Axios expects
    std::string header = "X-XSRF-TOKEN";
    // Requests that skip the check: an exact path, or a prefix ending in "*". For endpoints that
    // are called by machines (webhooks) and cannot hold a session token.
    std::vector<std::string> exceptPaths;
    // Behind a TLS-terminating proxy that sets X-Forwarded-Proto / X-Forwarded-Host: use their
    // first value for DrogonRequest::fullUrl() (the 409 X-Inertia-Location) and for the Secure
    // flag of the token cookie. Off by default: a client can forge these headers when the server
    // is reachable directly.
    bool trustForwardedHeaders = false;
};

// Wires an Inertia instance into the application; call once, after app().enableSession() and
// before app().run(). A second call throws std::logic_error. Drogon cannot report whether
// sessions are enabled, so when CSRF is enabled a mutating request (not exempt) that arrives
// without one is answered with a 500 that says to call app().enableSession() (instead of a
// confusing 419). Other requests are left alone; render() reports the missing session itself.
//  - pre-handling advice: Inertia::handleVersion (a version mismatch answers 409 here), then, for
//    every method but GET/HEAD/OPTIONS, the CSRF check: the header must equal the token kept in
//    the session (constant-time comparison), else the answer is 419 ("page expired");
//  - post-handling advice: Inertia::finalize and, when the client lacks the current token, the
//    XSRF-TOKEN cookie. Both are also applied to responses short-circuited by the pre-handling
//    advice (Drogon skips post-handling advices for those).
// The instance must be fully configured (shared props registered) before it is installed.
void install(const std::shared_ptr<const Inertia> &inertia, CsrfOptions csrf = {});

// Renders a page for `req` with the installed Inertia and the request's Drogon session (so
// app().enableSession() is required). `callback` receives exactly one response, possibly from
// another thread when SSR is enabled. An exception thrown while resolving props is logged and
// answered with a 500; the server keeps serving.
void render(const ::drogon::HttpRequestPtr &req, std::string component, Props props,
            std::function<void(const ::drogon::HttpResponsePtr &)> &&callback,
            RenderOptions opts = {});

// External redirect via the installed Inertia: 409 + X-Inertia-Location for Inertia requests,
// else a 302.
[[nodiscard]] Response location(const ::drogon::HttpRequestPtr &req, std::string_view url);

} // namespace inertia::drogon
