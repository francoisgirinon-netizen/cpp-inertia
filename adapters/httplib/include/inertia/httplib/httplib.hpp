#pragma once

#include <inertia/attributes.hpp>
#include <inertia/httplib/export.hpp>
#include <inertia/inertia.hpp>

#include <inertia/httplib/detail/httplib_include.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/// @brief Adapter that serves Inertia pages from a cpp-httplib server.
///
/// Inside this namespace the global cpp-httplib namespace must always be spelled `::httplib`.
namespace inertia::httplib {

/// @brief Read-only view of a cpp-httplib request.
///
/// It borrows the request: keep it alive for as long as the view is used. Not thread safe, like the
/// request it wraps.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_HTTPLIB_EXPORT HttplibRequest final : public RequestView {
  public:
    /// @param req The request to view; must outlive this object.
    /// @param trustForwardedHeaders Believe X-Forwarded-Proto and X-Forwarded-Host (first value of
    /// the list) when building fullUrl() and deciding isSecure(). Enable it only behind a reverse
    /// proxy that sets or strips those headers; a directly exposed server must leave it off, since
    /// any client can forge them.
    explicit HttplibRequest(const ::httplib::Request &req, bool trustForwardedHeaders = false);

    /// The method as received, such as "GET".
    /// @return The request method.
    [[nodiscard]] std::string_view method() const override;
    /// The request target exactly as received (path and raw query). An absolute-form target
    /// ("GET http://host/x") is reduced to its origin-relative part ("/x"); the asterisk form
    /// ("OPTIONS *") stays "*" and fullUrl() then is just scheme://host. A request that was not
    /// parsed off the wire (no raw target) falls back to its decoded path and re-encoded params.
    /// @return The request target relative to the origin.
    [[nodiscard]] std::string url() const override;
    /// The absolute URL of the request: `https://` when isSecure(), else `http://`, then the host
    /// and url().
    /// @return The absolute request URL.
    [[nodiscard]] std::string fullUrl() const override;
    /// Several header lines of one name are joined with ", ". fullUrl() ignores a Host header that
    /// cannot be a URL authority (spaces, '/', '@', ...) and uses "localhost" instead.
    /// @param name Header name, matched case-insensitively.
    /// @return The header value, or std::nullopt when the request does not carry it.
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override;

    /// True when a trusted X-Forwarded-Proto says https. The server itself speaks plain HTTP.
    /// @return Whether the client's connection to the proxy was secure.
    [[nodiscard]] bool isSecure() const;

  private:
    const ::httplib::Request *req_;
    std::string method_;
    bool trustForwarded_;
};
INERTIA_DLL_INTERFACE_END

/// Replaces the status, headers and body of `to`. Content-Length is dropped (cpp-httplib frames the
/// body itself); several headers of one name (Set-Cookie) all survive.
/// @param from The Inertia response to copy.
/// @param to The cpp-httplib response to overwrite.
INERTIA_HTTPLIB_EXPORT void toHttplib(const Response &from, ::httplib::Response &to);

/// @param res The cpp-httplib response to copy.
/// @return The status, headers and body of `res` as an Inertia response.
[[nodiscard]] INERTIA_HTTPLIB_EXPORT Response fromHttplib(const ::httplib::Response &res);

/// @brief The values of one session.
///
/// Opaque: reach it through HttplibSession.
class SessionData;

/// @brief Configuration of MemorySessionStore.
struct MemorySessionOptions {
    /// A session that is not used for this long is forgotten. Each open() of it renews the timer.
    std::chrono::seconds idleTtl{30 * 60};
    /// Upper bound on live sessions; values below 1 are raised to 1. At the cap the least recently
    /// used session is evicted to make room, so anonymous floods cannot grow memory without bound
    /// (they only push out old sessions).
    std::size_t maxSessions = 100'000;
    /// Name of the session cookie.
    std::string cookieName = "inertia_session";
    /// Time source; tests replace it. Default: std::chrono::steady_clock::now.
    std::function<std::chrono::steady_clock::time_point()> clock = nullptr;
};

/// @brief In-memory session store for the cpp-httplib adapter.
///
/// cpp-httplib has no sessions, so the adapter keeps them in memory: a map from a random id (the
/// session cookie) to the session's values. Thread safe. SINGLE PROCESS ONLY: sessions are lost on
/// restart and are not shared between processes or hosts; put a sticky-session proxy in front or
/// use another adapter when you need that. Memory is bounded by the idle TTL and by
/// MemorySessionOptions::maxSessions (least recently used first). Expired sessions are dropped,
/// oldest first, whenever a session is opened or rotated, so there is no sweep to schedule.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_HTTPLIB_EXPORT MemorySessionStore {
  public:
    /// @brief Result of open() and rotate().
    struct Opened {
        /// The session id, to be sent back to the client in the session cookie.
        std::string id;
        /// The session's values, shared with the store.
        std::shared_ptr<SessionData> data;
        /// False when `id` was presented by the client and is still alive; true when a new id was
        /// minted.
        bool created = false;
    };

    /// @param options Idle TTL, capacity and cookie settings.
    explicit MemorySessionStore(MemorySessionOptions options = {});
    /// Releases the store. Sessions still referenced by a request remain valid until released.
    ~MemorySessionStore();
    /// Not copyable: the store owns the sessions that requests share.
    MemorySessionStore(const MemorySessionStore &) = delete;
    /// Not copy-assignable.
    MemorySessionStore &operator=(const MemorySessionStore &) = delete;
    /// Not movable.
    MemorySessionStore(MemorySessionStore &&) = delete;
    /// Not move-assignable.
    MemorySessionStore &operator=(MemorySessionStore &&) = delete;

    /// The live session of `id` (renewing its idle timer), else a brand-new session under a fresh
    /// random id: an id the client sends that is unknown, expired or malformed is never adopted
    /// (no session fixation).
    /// @param id The session id presented by the client, or std::nullopt when it sent none.
    /// @return The session and whether its id was newly minted.
    /// @throws std::runtime_error when the system has no secure randomness.
    [[nodiscard]] Opened open(std::optional<std::string_view> id);

    /// Moves the session of `oldId` to a new random id: same data, the old id stops working at
    /// once. Call it when privileges change (after a login) to defeat session fixation.
    /// @param oldId The current id of the session.
    /// @return The session under its new id, with created = true. An unknown or expired `oldId`
    /// yields a fresh, empty session.
    /// @throws std::runtime_error when the system has no secure randomness.
    [[nodiscard]] Opened rotate(std::string_view oldId);

    /// Drops every session idle for longer than the TTL. open() and rotate() already drop expired
    /// sessions, so this only matters to free memory while no request arrives.
    void sweep();
    /// @return The number of sessions held, including expired ones not yet dropped.
    [[nodiscard]] std::size_t size() const;

    /// @return The configured session cookie name.
    [[nodiscard]] const std::string &cookieName() const { return options_.cookieName; }
    /// The Set-Cookie value that hands `id` to the browser: HttpOnly, SameSite=Lax, Path=/, and
    /// Secure when the request came over HTTPS.
    /// @param id The session id to set.
    /// @param secure Whether the request came over HTTPS.
    /// @return The Set-Cookie header value.
    [[nodiscard]] std::string cookieHeader(std::string_view id, bool secure) const;

  private:
    struct Entry {
        std::shared_ptr<SessionData> data;
        std::chrono::steady_clock::time_point lastUsed;
        std::list<std::string>::iterator position; // in lru_
    };

    [[nodiscard]] std::chrono::steady_clock::time_point now() const;
    void expireLocked(std::chrono::steady_clock::time_point at);
    Opened insertLocked(std::string id, std::shared_ptr<SessionData> data,
                        std::chrono::steady_clock::time_point at);

    MemorySessionOptions options_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> sessions_; // guarded by mutex_
    std::list<std::string> lru_; // ids, most recently used first; guarded by mutex_
};
INERTIA_DLL_INTERFACE_END

/// @brief One request's view of a session.
///
/// Safe to use while other requests of the same session run (the values are guarded by a
/// per-session lock). Values are held as Json in memory.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_HTTPLIB_EXPORT HttplibSession final : public SessionStore {
  public:
    /// @param data The session values, as held by MemorySessionStore::Opened::data.
    explicit HttplibSession(std::shared_ptr<SessionData> data);

    /// @param key The key to look up.
    /// @return The stored value, or std::nullopt when `key` is absent.
    [[nodiscard]] std::optional<Json> get(std::string_view key) const override;
    /// Stores `value` under `key`, replacing any previous value.
    /// @param key The key to store under.
    /// @param value The value to store.
    void put(std::string key, Json value) override;
    /// Removes `key` from the session.
    /// @param key The key to remove.
    /// @return The removed value, or std::nullopt when `key` was absent.
    std::optional<Json> pull(std::string_view key) override;

    /// Atomically returns the value of `key`, storing make() first when there is none. make() runs
    /// while the session lock is held, so it must not call any method of this session.
    /// @param key The key to read or create.
    /// @param make Produces the value to store when `key` has none.
    /// @return The value stored under `key` after the call.
    [[nodiscard]] Json getOrCreate(std::string_view key, const std::function<Json()> &make);

  private:
    std::shared_ptr<SessionData> data_;
};
INERTIA_DLL_INTERFACE_END

/// @brief Blocking HTTP client for server-side rendering, on cpp-httplib's client.
///
/// get() and post() perform the request on the calling thread and call `done` exactly once before
/// returning; nothing throws. The call blocks the request-handling thread (cpp-httplib is
/// thread-per-connection) for up to the SSR timeout: connection, read and write are each bounded by
/// it, and so is the whole exchange. `done` runs on the calling thread: an exception it throws is
/// not caught here and propagates out of get()/post() (the client's own failures never do), so
/// `done` must not throw. Only http:// URLs work (the adapter is built without TLS); https reports
/// an error. Thread safe: a fresh ::httplib::Client is created per call, so concurrent requests
/// never share a socket.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_HTTPLIB_EXPORT HttplibHttpClient final : public HttpClient {
  public:
    /// Sends a JSON POST and reports the answer through `done`.
    /// @param url The absolute http:// URL to post to.
    /// @param jsonBody The body, sent with Content-Type application/json.
    /// @param timeout Bound on connection, read, write and the whole exchange.
    /// @param done Invoked once, on this thread, with the status and body or an error (status 0).
    void post(std::string url, std::string jsonBody, std::chrono::milliseconds timeout,
              std::function<void(HttpResult)> done) override;
    /// Sends a GET and reports the answer through `done`.
    /// @param url The absolute http:// URL to fetch.
    /// @param timeout Bound on connection, read, write and the whole exchange.
    /// @param done Invoked once, on this thread, with the status and body or an error (status 0).
    void get(std::string url, std::chrono::milliseconds timeout,
             std::function<void(HttpResult)> done) override;
};
INERTIA_DLL_INTERFACE_END

/// @brief CSRF protection for unsafe methods, as a double-submit cookie.
///
/// Every method other than GET, HEAD and OPTIONS must send the token in the header, and the token
/// must equal the one kept in the session. The XSRF cookie is set on the responses of requests that
/// opened a session.
struct CsrfOptions {
    /// Turns the check and the XSRF cookie on or off.
    bool enabled = true;
    /// Name of the cookie that carries the token. Readable by scripts (not HttpOnly), as Axios
    /// expects.
    std::string cookie = "XSRF-TOKEN";
    /// Name of the request header that must echo the token.
    std::string header = "X-XSRF-TOKEN";
    /// Requests that skip the check: an exact path, or a prefix ending in "*" (a bare "*" exempts
    /// every path). For endpoints that are called by machines (webhooks, the SSR server) and cannot
    /// hold a session token.
    std::vector<std::string> exceptPaths;
};

/// @brief Settings for install().
struct Options {
    /// CSRF protection; see CsrfOptions.
    CsrfOptions csrf;
    /// Believe X-Forwarded-Proto / X-Forwarded-Host (see HttplibRequest). Only behind a reverse
    /// proxy that sets or strips them; it also makes the session and XSRF cookies Secure for
    /// requests that arrived over https.
    bool trustForwardedHeaders = false;
    /// Where sessions live; null creates a MemorySessionStore with default options. Shared by every
    /// request of the server.
    std::shared_ptr<MemorySessionStore> sessions = nullptr;
    /// Your own pre-routing and post-routing logic. cpp-httplib has a single slot for each, and
    /// install() occupies both, so NEVER call Server::set_pre_routing_handler or
    /// set_post_routing_handler yourself after install(): that silently switches off the version
    /// check, CSRF, session cookies and Vary finalization. Use these hooks instead.
    ///  - preRouting runs after Inertia's own pre-routing step and only when that step did not
    ///    answer (a 409 or 419 never reaches it). Return Handled to answer the request yourself;
    ///    the response is then finalized like any other.
    ///  - postRouting runs before Inertia's post-routing step, so Inertia's cookies and
    ///    Inertia::finalize (Vary) always see the final response.
    /// Exceptions thrown by a hook are logged; in preRouting they turn into a 500, and in
    /// postRouting they skip the rest of the adapter's post-routing step.
    std::function<::httplib::Server::HandlerResponse(const ::httplib::Request &,
                                                     ::httplib::Response &)>
        preRouting = nullptr;
    /// Runs before Inertia's post-routing step, so Inertia's cookies and finalize always see the
    /// final response. Exceptions are handled as for preRouting; the rest of the adapter's
    /// post-routing step is then skipped.
    std::function<void(const ::httplib::Request &, ::httplib::Response &)> postRouting = nullptr;
    /// Receives the adapter's error messages (a failed handler, a failed render); null writes them
    /// to stderr. Called from request-handling threads.
    std::function<void(std::string_view)> logError = nullptr;
};

/// Wires an Inertia instance into a cpp-httplib server. Call once per server, before listen() and
/// after the instance is fully configured (shared props registered). Replaces any pre-routing and
/// post-routing handler already set on the server (pass them as Options::preRouting /
/// postRouting to keep them), and must be the last to set those two handlers: see Options.
///
/// Sessions are created lazily: a request whose handler never calls session() or render(), and that
/// needs no CSRF check, gets no session and no cookie.
///  - pre-routing: opens the session (cookie `inertia_session`), then Inertia::handleVersion (a
///    version mismatch answers 409 here), then, for every method but GET/HEAD/OPTIONS, the CSRF
///    check: the header must equal the token kept in the session (constant-time comparison), else
///    the answer is 419 ("page expired"). The 419 answer already carries the session cookie and a
///    token. An exception here answers 500.
///  - post-routing: the session cookie for a new session, the XSRF-TOKEN cookie when the client
///    lacks the current token, and Inertia::finalize. cpp-httplib runs it for every response,
///    including the ones the pre-routing handler short-circuited and 404/500 answers.
///
/// Handlers must be registered on the same server; the Inertia, the session store and the options
/// are shared by all requests (the Inertia is only read). cpp-httplib runs the handlers on its
/// worker threads, so the shared state is accessed concurrently.
///
/// @code{.cpp}
/// auto app = std::make_shared<const inertia::Inertia>(config);
/// ::httplib::Server server;
/// inertia::httplib::install(server, app);
/// server.Get("/users", [](const ::httplib::Request &req, ::httplib::Response &res) {
///     inertia::httplib::render(req, res, "Users/Index", props);
/// });
/// server.listen("0.0.0.0", 8080);
/// @endcode
///
/// @param server The server to install the handlers on.
/// @param inertia The Inertia instance that renders the pages.
/// @param options Sessions, CSRF, hooks and logging; see Options.
/// @throws std::logic_error when the server already has an installation.
/// @throws std::invalid_argument when `inertia` is null.
INERTIA_HTTPLIB_EXPORT void install(::httplib::Server &server,
                                    const std::shared_ptr<const Inertia> &inertia,
                                    Options options = {});

/// Renders a page into `res` with the Inertia installed on the server and the request's session.
/// Blocks until the response is ready (SSR included). An exception thrown while resolving props is
/// logged and answered with a 500; the server keeps serving.
/// @param req The request being handled.
/// @param res The response of that request; it receives the page.
/// @param component The page component name, as the front end registers it.
/// @param props The props of the page.
/// @param opts Per-render overrides of encryptHistory and preserveBigIntegers; unset fields keep
/// the values of the Config.
/// @throws std::logic_error when no install() ran for the server handling this request.
INERTIA_HTTPLIB_EXPORT void render(const ::httplib::Request &req, ::httplib::Response &res,
                                   std::string component, Props props, RenderOptions opts = {});

/// External redirect via the installed Inertia, written into `res`: 409 + X-Inertia-Location for
/// Inertia requests, else a 302.
/// @param req The request being handled.
/// @param res The response of that request.
/// @param url The external URL to redirect to.
/// @throws std::logic_error when no install() ran for the server handling this request.
INERTIA_HTTPLIB_EXPORT void location(const ::httplib::Request &req, ::httplib::Response &res,
                                     std::string_view url);

/// The Inertia session of the request being handled (the one the session cookie names). The first
/// call of a request starts the session when the client has none (or an unknown id). Each call
/// returns a new view of the same session.
/// @param res The response of the request being handled.
/// @return A view of the request's session.
/// @throws std::logic_error when no install() ran for the server handling this request.
[[nodiscard]] INERTIA_HTTPLIB_EXPORT HttplibSession session(const ::httplib::Response &res);

/// Gives the request's session a new random id (same data, old id dead) and a new CSRF token, and
/// emits the new cookies. Call it when privileges change, e.g. right after a successful login, so
/// that an id planted before the login (session fixation) is worthless.
/// @param res The response of the request being handled.
/// @throws std::logic_error when no install() ran for the server handling this request.
INERTIA_HTTPLIB_EXPORT void regenerateSession(const ::httplib::Response &res);

} // namespace inertia::httplib
