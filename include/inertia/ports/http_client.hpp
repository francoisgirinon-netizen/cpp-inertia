#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>

namespace inertia {

struct HttpResult {
    int status = 0;
    std::string body;
    std::optional<std::string> error;
};

class HttpClient {
  protected:
    HttpClient() = default;
    HttpClient(const HttpClient &) = default;
    HttpClient &operator=(const HttpClient &) = default;
    HttpClient(HttpClient &&) = default;
    HttpClient &operator=(HttpClient &&) = default;

  public:
    virtual ~HttpClient() = default;

    virtual void post(std::string url, std::string jsonBody, std::chrono::milliseconds timeout,
                      std::function<void(HttpResult)> done) = 0;
    virtual void get(std::string url, std::chrono::milliseconds timeout,
                     std::function<void(HttpResult)> done) = 0;
};

} // namespace inertia
