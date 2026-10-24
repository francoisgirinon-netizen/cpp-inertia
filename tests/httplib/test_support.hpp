#pragma once

#include <httplib.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// Support shared by the adapter tests and the conformance harness: a cpp-httplib server on an
// ephemeral loopback port running in a background thread, and a blocking client with a cookie jar.
namespace inertia::httplib_test {

// Owns a ::httplib::Server. Register routes (and call inertia::httplib::install) on server()
// before start(); the destructor stops the server and joins its thread.
class TestServer {
  public:
    TestServer();
    ~TestServer();
    TestServer(const TestServer &) = delete;
    TestServer &operator=(const TestServer &) = delete;
    TestServer(TestServer &&) = delete;
    TestServer &operator=(TestServer &&) = delete;

    [[nodiscard]] ::httplib::Server &server() { return server_; }
    // Binds 127.0.0.1 on a free port and serves in a background thread; returns once the server
    // accepts connections. Throws std::runtime_error when it cannot start.
    void start();
    [[nodiscard]] std::uint16_t port() const { return port_; }
    [[nodiscard]] std::string baseUrl() const;

  private:
    ::httplib::Server server_;
    std::thread thread_;
    std::uint16_t port_ = 0;
};

struct Reply {
    int status = 0;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    // Case-insensitive lookup of the first matching header; "" when absent.
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

// A blocking HTTP client with a cookie jar, bound to one server. Not thread safe: use one per
// thread. A Cookie header set explicitly on a request replaces the jar for that request.
class Client {
  public:
    explicit Client(std::uint16_t port);

    [[nodiscard]] Reply send(const Request &req);
    [[nodiscard]] Reply get(std::string url) {
        Request req;
        req.url = std::move(url);
        return send(req);
    }
    // Forgets every cookie (and with them the session).
    void resetCookies() { jar_.clear(); }
    [[nodiscard]] const std::map<std::string, std::string> &cookies() const { return jar_; }

  private:
    void rememberCookie(const std::string &setCookie);

    std::uint16_t port_;
    std::map<std::string, std::string> jar_;
};

// Sends `request` (the exact bytes, so it can carry a request target that cpp-httplib's own client
// would normalize) to 127.0.0.1:`port`, and returns everything the server answers until it closes
// the connection. Include "Connection: close" in the request.
[[nodiscard]] std::string rawExchange(std::uint16_t port, const std::string &request);

} // namespace inertia::httplib_test
