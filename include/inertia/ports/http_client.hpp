#pragma once

#include <inertia/core_export.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>

namespace inertia {

/// Outcome of one HttpClient exchange.
struct HttpResult {
    /// HTTP status code of the response, or 0 when no response was received.
    int status = 0;
    /// Response body; empty when no response was received.
    std::string body;
    /// Description of the transport failure (connection refused, timeout, ...), or std::nullopt
    /// when a response was received, whatever its status.
    std::optional<std::string> error;
};

/// Asynchronous HTTP client used for server-side rendering (SSR).
///
/// Needed only when `Config::ssr.enabled` is set. Implementations must be thread safe: renders run
/// concurrently and each one may issue a request.
class INERTIA_CORE_EXPORT HttpClient {
  protected:
    HttpClient() = default;
    /// Copy-constructs a client in a derived class.
    HttpClient(const HttpClient &) = default;
    /// Copy-assigns a client in a derived class.
    ///
    /// @return `*this`.
    HttpClient &operator=(const HttpClient &) = default;
    /// Move-constructs a client in a derived class.
    HttpClient(HttpClient &&) = default;
    /// Move-assigns a client in a derived class.
    ///
    /// @return `*this`.
    HttpClient &operator=(HttpClient &&) = default;

  public:
    /// Virtual so that clients can be destroyed through an HttpClient pointer.
    virtual ~HttpClient() = default;

    /// Sends a POST request with a JSON body.
    ///
    /// Implementations must call `done` exactly once, on any thread, and must not throw. A failure
    /// is reported as an HttpResult with `error` set, never as an exception. The timeout bounds the
    /// whole exchange, connection included. `done` may be called before this function returns.
    ///
    /// @param url Absolute URL of the endpoint, such as "http://127.0.0.1:13714/render".
    /// @param jsonBody Request body, sent as JSON.
    /// @param timeout Upper bound for the whole exchange.
    /// @param done Completion callback, invoked exactly once with the outcome.
    virtual void post(std::string url, std::string jsonBody, std::chrono::milliseconds timeout,
                      std::function<void(HttpResult)> done) = 0;

    /// Sends a GET request.
    ///
    /// The same guarantees as post() apply: `done` is called exactly once, the call does not throw,
    /// and the timeout bounds the whole exchange.
    ///
    /// @param url Absolute URL of the endpoint.
    /// @param timeout Upper bound for the whole exchange.
    /// @param done Completion callback, invoked exactly once with the outcome.
    virtual void get(std::string url, std::chrono::milliseconds timeout,
                     std::function<void(HttpResult)> done) = 0;
};

} // namespace inertia
