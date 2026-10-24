#pragma once

#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Drogon's app() is a process singleton that can run only once, so every Drogon test executable
// starts one server for the whole process (a Catch2 listener) and talks to it over real HTTP.
namespace inertia::drogon_test {

// Implemented by each test executable: enables sessions, installs the adapter and registers the
// handlers. Called once, on the main thread, before the server starts.
void configureApp();

// How long GET /slow takes to answer.
inline constexpr std::chrono::milliseconds kSlowDelay{1000};

// The port of the running server (127.0.0.1).
[[nodiscard]] std::uint16_t port();

struct Reply {
    int status = 0;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    // Case-insensitive lookup of the first matching header.
    [[nodiscard]] std::string header(std::string_view name) const;
    // The whole Set-Cookie line that sets `name`, or "" when the reply does not set it.
    [[nodiscard]] std::string setCookie(std::string_view name) const;
    // The value that Set-Cookie line assigns, or "".
    [[nodiscard]] std::string cookieValue(std::string_view name) const;
};

struct Request {
    std::string method = "GET";
    std::string url = "/"; // path and query, sent verbatim
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

// A blocking HTTP client with a cookie jar. Do not use it from an event-loop thread.
class Client {
  public:
    Client();

    [[nodiscard]] Reply send(const Request &req);
    [[nodiscard]] Reply get(std::string url) {
        Request req;
        req.url = std::move(url);
        return send(req);
    }
    // Forgets every cookie (and with them the session).
    void resetCookies();

  private:
    trantor::EventLoopThread loopThread_{"test-client"};
    ::drogon::HttpClientPtr client_;
};

} // namespace inertia::drogon_test
