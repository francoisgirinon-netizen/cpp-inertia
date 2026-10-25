#pragma once

#include <inertia/attributes.hpp>
#include <inertia/drogon/export.hpp>
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

/// Adapter that connects Inertia to the Drogon web framework.
///
/// Install it once with install() after app().enableSession(), then answer each route with render()
/// or location(). Inside this namespace the global Drogon namespace must always be spelled
/// `::drogon`.
namespace inertia::drogon {

/// Read-only view of a Drogon request.
///
/// The view borrows the request it wraps, so it is valid only while that request is alive. It is
/// not thread safe, like the request.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_DROGON_EXPORT DrogonRequest final : public RequestView {
  public:
    /// Creates a view of a Drogon request.
    ///
    /// @param req The request to view. Must not be null.
    /// @param trustForwardedHeaders Take the scheme and host from the first value of
    /// X-Forwarded-Proto and X-Forwarded-Host. Enable only behind a proxy that sets and overwrites
    /// those headers: a client that reaches the server directly can forge them.
    explicit DrogonRequest(::drogon::HttpRequestPtr req, bool trustForwardedHeaders = false);

    /// The method of the request, in uppercase.
    ///
    /// @return "GET", "POST", ... as sent by the client.
    [[nodiscard]] std::string_view method() const override;

    /// The original (not URL-decoded) path, followed by the query string when there is one.
    ///
    /// @return The request target, such as "/users?page=2".
    [[nodiscard]] std::string url() const override;

    /// The absolute URL of the request.
    ///
    /// The scheme follows isSecure(). The host comes from the Host header, or from the first value
    /// of X-Forwarded-Host when forwarded headers are trusted and that value is a plausible host
    /// name. Falls back to "localhost" when no host is known.
    ///
    /// @return `scheme://host` followed by url().
    [[nodiscard]] std::string fullUrl() const override;

    /// Looks up a request header, case-insensitively.
    ///
    /// @param name Header name, in any case.
    /// @return The value, or std::nullopt when the request does not carry the header.
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override;

    /// Whether the request arrived over HTTPS.
    ///
    /// Without trusted forwarded headers, this is the connection's own security. With them, the
    /// first value of X-Forwarded-Proto wins when it is "https" or "http"; any other value, or no
    /// header, falls back to the connection.
    ///
    /// @return true when the request was made over HTTPS.
    [[nodiscard]] bool isSecure() const;

  private:
    ::drogon::HttpRequestPtr req_;
    bool trustForwarded_;
};
INERTIA_DLL_INTERFACE_END

/// Converts an Inertia response to a Drogon response.
///
/// Keeps the status and body. Repeated headers are combined with ", " because Drogon stores one
/// value per name; headers that cannot be combined keep only the last value. Each Set-Cookie line
/// becomes a separate cookie.
///
/// @param res The Inertia response to convert.
/// @return A new Drogon response carrying the same status, headers and body.
[[nodiscard]] INERTIA_DROGON_EXPORT ::drogon::HttpResponsePtr toDrogon(const Response &res);

/// Converts an Inertia response to a Drogon response, moving its body instead of copying it.
///
/// Same conversion as the overload for a const response. Prefer it when the response is not
/// needed afterwards: a page body can be hundreds of kilobytes.
///
/// @param res The Inertia response to convert. Its body is moved from.
/// @return A new Drogon response carrying the same status, headers and body.
[[nodiscard]] INERTIA_DROGON_EXPORT ::drogon::HttpResponsePtr toDrogon(Response &&res);

/// Converts a Drogon response to an Inertia response.
///
/// Keeps every header line, including each cookie as its own Set-Cookie header, in the order the
/// adapter reads them.
///
/// @param res The Drogon response to convert.
/// @return The Inertia response with the same status, headers and body.
[[nodiscard]] INERTIA_DROGON_EXPORT Response fromDrogon(const ::drogon::HttpResponsePtr &res);

/// Session store backed by a Drogon session.
///
/// Each value is kept as its dumped JSON text inside the Drogon session. Drogon's session has its
/// own lock, so each operation is atomic even while other requests of the same visitor run.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_DROGON_EXPORT DrogonSession final : public SessionStore {
  public:
    /// Wraps a Drogon session.
    ///
    /// @param session The session, from `req->session()`. Must not be null.
    /// @throws std::invalid_argument When `session` is null, which means that sessions are not
    /// enabled: call app().enableSession() before app().run().
    explicit DrogonSession(::drogon::SessionPtr session);

    /// Reads a value.
    ///
    /// Stored text that is not valid JSON reads as absent instead of throwing.
    ///
    /// @param key Name of the entry.
    /// @return The stored value, or std::nullopt when the key is absent or its text is not JSON.
    [[nodiscard]] std::optional<Json> get(std::string_view key) const override;

    /// Stores a value, replacing any previous one.
    ///
    /// @param key Name of the entry.
    /// @param value JSON value to store.
    void put(std::string key, Json value) override;

    /// Reads a value and removes it, atomically.
    ///
    /// The entry is removed even when its text is not valid JSON, in which case the result is
    /// std::nullopt.
    ///
    /// @param key Name of the entry.
    /// @return The value that was stored, or std::nullopt when the key was absent or not JSON.
    std::optional<Json> pull(std::string_view key) override;

  private:
    ::drogon::SessionPtr session_;
};
INERTIA_DLL_INTERFACE_END

/// Server-side rendering transport built on Drogon's HTTP client.
///
/// Keeps one ::drogon::HttpClient per origin, created on first use and shared by every request.
/// Completion callbacks run on the client's event-loop thread.
///
/// Drogon cannot cancel a request and does not pipeline, so a hung SSR server would stall every
/// later request queued on the same connection. After a timeout the origin's client is therefore
/// dropped from the pool (at most once per timeout window) and the next request opens a fresh
/// connection. The old client lives on until Drogon is done with its pending request.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_DROGON_EXPORT DrogonHttpClient final : public HttpClient {
  public:
    /// Creates a client.
    ///
    /// @param loop Event loop that runs the connections. nullptr uses the application's event loop.
    explicit DrogonHttpClient(trantor::EventLoop *loop = nullptr);

    /// Sends a POST request with a JSON body.
    ///
    /// Calls `done` exactly once. The outcome of a sent request arrives on the event-loop thread.
    /// An invalid URL is reported at once, on the calling thread, with status 0 and an error
    /// message. A timeout is reported with status 0 and the name of Drogon's result as the error.
    ///
    /// @param url Absolute URL, such as "http://127.0.0.1:13714/render".
    /// @param jsonBody Request body, sent with the content type application/json.
    /// @param timeout Upper bound for the whole exchange.
    /// @param done Completion callback, invoked exactly once with the outcome.
    void post(std::string url, std::string jsonBody, std::chrono::milliseconds timeout,
              std::function<void(HttpResult)> done) override;

    /// Sends a GET request.
    ///
    /// Reports failures as post() does: `done` is called exactly once on the event-loop thread.
    ///
    /// @param url Absolute URL of the endpoint.
    /// @param timeout Upper bound for the whole exchange.
    /// @param done Completion callback, invoked exactly once with the outcome.
    void get(std::string url, std::chrono::milliseconds timeout,
             std::function<void(HttpResult)> done) override;

  private:
    /// Sends one request of the given method, shared by post() and get().
    void send(::drogon::HttpMethod method, std::string_view url, std::string body,
              std::chrono::milliseconds timeout, std::function<void(HttpResult)> done);

    // Shared with the in-flight callbacks, which may outlive this object.
    struct Pool;

    trantor::EventLoop *loop_;
    std::shared_ptr<Pool> pool_;
};
INERTIA_DLL_INTERFACE_END

/// CSRF protection settings for install().
///
/// The check follows the convention that Axios expects. The token is created on the first response
/// of a session, kept in the session under "inertia.csrf_token", and sent to the client in the
/// cookie named by `cookie`. Every request other than GET, HEAD and OPTIONS must echo it in the
/// `header` request header.
struct CsrfOptions {
    /// Whether the check runs. When false, neither the check nor the token cookie is applied.
    bool enabled = true;
    /// Name of the token cookie. It is not HttpOnly, so scripts can read it and echo it back.
    std::string cookie = "XSRF-TOKEN";
    /// Name of the request header that must carry the token.
    std::string header = "X-XSRF-TOKEN";
    /// Paths that skip the check: an exact path, or a prefix ending in "*" (such as "/webhooks/*").
    /// Matching uses the path without the query string. Use them for endpoints called by machines,
    /// which cannot hold a session token.
    std::vector<std::string> exceptPaths;
    /// Trust X-Forwarded-Proto and X-Forwarded-Host, set by a TLS-terminating proxy.
    ///
    /// When true, the first value of each header is used for DrogonRequest::fullUrl() (and so for
    /// the 409 X-Inertia-Location), and for the Secure flag of the token cookie. Off by default: a
    /// client can forge these headers when the server is reachable directly.
    bool trustForwardedHeaders = false;
};

/// Wires an Inertia instance into the Drogon application.
///
/// Call it once, after app().enableSession() and before app().run(). The advices are registered on
/// the application, in this order:
///  - before the handler: Inertia::handleVersion, which answers a stale X-Inertia-Version with 409;
///    then, for every method but GET, HEAD and OPTIONS, the CSRF check, which answers 419 ("page
///    expired") when the header does not match the session's token (compared in constant time);
///  - after the handler: Inertia::finalize, and the token cookie when the client does not present
///    the current token.
///
/// The after-handler steps also run on responses produced before the handler, since Drogon skips
/// its after-handler advices for those.
///
/// When CSRF is enabled, a mutating request that is not exempt and arrives without a session is
/// answered with a 500 that says to call app().enableSession(), instead of a confusing 419. Drogon
/// offers no way to ask whether sessions are enabled, so this is how the mistake shows. Other
/// requests are left alone; render() reports the missing session itself.
///
/// The instance must be fully configured (shared props registered) before it is installed.
///
/// @param inertia The instance to install. Must not be null.
/// @param csrf CSRF and forwarded-header settings.
/// @throws std::invalid_argument When `inertia` is null.
/// @throws std::logic_error When install() was already called: an adapter is installed once per
/// process.
INERTIA_DROGON_EXPORT void install(const std::shared_ptr<const Inertia> &inertia,
                                   CsrfOptions csrf = {});

/// Renders a page for a Drogon request.
///
/// Uses the instance installed by install() and the request's session, so app().enableSession()
/// is required. Exceptions are never thrown to the caller. An exception thrown while resolving
/// props is logged and answered with a 500, and so is a missing session; the server keeps serving.
///
/// `callback` receives exactly one response. With SSR enabled it may run later, on another thread,
/// once the SSR server has answered or timed out.
///
/// @param req The request being answered.
/// @param component Name of the page component, such as "Users/Index".
/// @param props Props of the page; resolved before the call returns, as in Inertia::render().
/// @param callback Receives the finished response, exactly once. Moved into the adapter.
/// @param opts Rendering options, such as SSR or history encryption.
INERTIA_DROGON_EXPORT void render(const ::drogon::HttpRequestPtr &req, std::string component,
                                  Props props,
                                  std::function<void(const ::drogon::HttpResponsePtr &)> &&callback,
                                  RenderOptions opts = {});

/// Builds an external redirect with the installed Inertia instance.
///
/// For an Inertia request this is a 409 with an X-Inertia-Location header, which makes the client
/// perform a full-page visit. Any other request gets a plain 302.
///
/// @param req The request being answered.
/// @param url Target URL of the redirect.
/// @return The redirect response, as an Inertia response.
/// @throws std::logic_error When install() has not been called.
[[nodiscard]] INERTIA_DROGON_EXPORT Response location(const ::drogon::HttpRequestPtr &req,
                                                      std::string_view url);

} // namespace inertia::drogon
