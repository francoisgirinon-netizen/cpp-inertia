#pragma once

#include <inertia/inertia.hpp>

#include <httplib.h>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

// Inside this namespace the global cpp-httplib namespace must always be spelled ::httplib.
namespace inertia::httplib {

// Read-only view of a cpp-httplib request. It borrows the request: keep it alive for as long as the
// view is used. Not thread safe, like the request it wraps.
class HttplibRequest final : public RequestView {
  public:
    // `trustForwardedHeaders`: believe X-Forwarded-Proto and X-Forwarded-Host (first value of the
    // list) when building fullUrl() and deciding isSecure(). Enable it only behind a reverse proxy
    // that sets or strips those headers; a directly exposed server must leave it off, since any
    // client can forge them.
    explicit HttplibRequest(const ::httplib::Request &req, bool trustForwardedHeaders = false);

    [[nodiscard]] std::string_view method() const override;
    // The request target exactly as received (path and raw query). A request that was not parsed
    // off the wire (no raw target) falls back to its decoded path and re-encoded parameters.
    [[nodiscard]] std::string url() const override;
    [[nodiscard]] std::string fullUrl() const override;
    // Several header lines of one name are joined with ", ".
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override;

    // True when a trusted X-Forwarded-Proto says https. The server itself speaks plain HTTP.
    [[nodiscard]] bool isSecure() const;

  private:
    const ::httplib::Request *req_;
    std::string method_;
    bool trustForwarded_;
};

// Replaces the status, headers and body of `to`. Content-Length is dropped (cpp-httplib frames the
// body itself); several headers of one name (Set-Cookie) all survive.
void toHttplib(const Response &from, ::httplib::Response &to);
[[nodiscard]] Response fromHttplib(const ::httplib::Response &res);

// The values of one session. Opaque: reach it through HttplibSession.
class SessionData;

struct MemorySessionOptions {
    // A session that was not used for this long is forgotten.
    std::chrono::seconds idleTtl{30 * 60};
    // Expired sessions are swept out lazily, at most this often, while sessions are opened.
    std::chrono::seconds sweepInterval{60};
    std::string cookieName = "inertia_session";
    // Time source; tests replace it. Default: std::chrono::steady_clock::now.
    std::function<std::chrono::steady_clock::time_point()> clock = nullptr;
};

// cpp-httplib has no sessions, so the adapter keeps them in memory: a map from a random id (the
// session cookie) to the session's values. Thread safe. SINGLE PROCESS ONLY: sessions are lost on
// restart and are not shared between processes or hosts; put a sticky-session proxy in front or
// use another adapter when you need that. Memory is bounded only by the idle TTL.
class MemorySessionStore {
  public:
    struct Opened {
        std::string id;
        std::shared_ptr<SessionData> data;
        bool created = false; // false: `id` was presented by the client and is still alive
    };

    explicit MemorySessionStore(MemorySessionOptions options = {});
    ~MemorySessionStore();
    MemorySessionStore(const MemorySessionStore &) = delete;
    MemorySessionStore &operator=(const MemorySessionStore &) = delete;
    MemorySessionStore(MemorySessionStore &&) = delete;
    MemorySessionStore &operator=(MemorySessionStore &&) = delete;

    // The live session of `id` (renewing its idle timer), else a brand-new session under a fresh
    // random id: an id the client sends that is unknown, expired or malformed is never adopted
    // (no session fixation). Throws std::runtime_error when the system has no secure randomness.
    [[nodiscard]] Opened open(std::optional<std::string_view> id);

    // Drops every session idle for longer than the TTL.
    void sweep();
    [[nodiscard]] std::size_t size() const;

    [[nodiscard]] const std::string &cookieName() const { return options_.cookieName; }
    // The Set-Cookie value that hands `id` to the browser: HttpOnly, SameSite=Lax, Path=/, and
    // Secure when the request came over HTTPS.
    [[nodiscard]] std::string cookieHeader(std::string_view id, bool secure) const;

  private:
    struct Entry {
        std::shared_ptr<SessionData> data;
        std::chrono::steady_clock::time_point lastUsed;
    };

    [[nodiscard]] std::chrono::steady_clock::time_point now() const;
    void sweepLocked(std::chrono::steady_clock::time_point at);

    MemorySessionOptions options_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> sessions_; // guarded by mutex_
    std::chrono::steady_clock::time_point lastSweep_; // guarded by mutex_
};

// One request's view of a session. Safe to use while other requests of the same session run (the
// values are guarded by a per-session lock). Values are held as Json in memory.
class HttplibSession final : public SessionStore {
  public:
    explicit HttplibSession(std::shared_ptr<SessionData> data);

    [[nodiscard]] std::optional<Json> get(std::string_view key) const override;
    void put(std::string key, Json value) override;
    std::optional<Json> pull(std::string_view key) override;

    // Atomically returns the value of `key`, storing make() first when there is none.
    [[nodiscard]] Json getOrCreate(std::string_view key, const std::function<Json()> &make);

  private:
    std::shared_ptr<SessionData> data_;
};

} // namespace inertia::httplib
