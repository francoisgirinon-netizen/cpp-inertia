#include <inertia/drogon/drogon.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace inertia::drogon {

namespace {

struct SplitUrl {
    std::string origin; // scheme://host[:port]
    std::string path;   // path and query, never empty
};

std::optional<SplitUrl> splitUrl(std::string_view url) {
    const auto schemeEnd = url.find("://");
    if (schemeEnd == std::string_view::npos || schemeEnd == 0) {
        return std::nullopt;
    }
    const auto pathStart = url.find('/', schemeEnd + 3);
    const auto origin = url.substr(0, pathStart);
    if (origin.size() <= schemeEnd + 3 || origin.find(' ') != std::string_view::npos) {
        return std::nullopt;
    }
    return SplitUrl{
        .origin = std::string{origin},
        .path = pathStart == std::string_view::npos ? "/" : std::string{url.substr(pathStart)}};
}

} // namespace

DrogonHttpClient::DrogonHttpClient(trantor::EventLoop *loop) : loop_(loop) {}

void DrogonHttpClient::post(std::string url, std::string jsonBody,
                            std::chrono::milliseconds timeout,
                            std::function<void(HttpResult)> done) {
    send(::drogon::Post, url, std::move(jsonBody), timeout, std::move(done));
}

void DrogonHttpClient::get(std::string url, std::chrono::milliseconds timeout,
                           std::function<void(HttpResult)> done) {
    send(::drogon::Get, url, {}, timeout, std::move(done));
}

::drogon::HttpClientPtr DrogonHttpClient::clientFor(const std::string &origin) {
    const std::scoped_lock lock{mutex_};
    auto &client = clients_[origin];
    if (!client) {
        client = ::drogon::HttpClient::newHttpClient(origin, loop_);
    }
    return client;
}

void DrogonHttpClient::send(::drogon::HttpMethod method, std::string_view url, std::string body,
                            std::chrono::milliseconds timeout,
                            std::function<void(HttpResult)> done) {
    const auto parts = splitUrl(url);
    if (!parts) {
        done({.status = 0, .body = {}, .error = "invalid URL: " + std::string{url}});
        return;
    }
    auto req = ::drogon::HttpRequest::newHttpRequest();
    req->setMethod(method);
    req->setPathEncode(false);
    req->setPath(parts->path);
    if (method == ::drogon::Post) {
        req->setContentTypeCode(::drogon::CT_APPLICATION_JSON);
        req->setBody(std::move(body));
    }
    const auto seconds = std::chrono::duration<double>(timeout).count();
    clientFor(parts->origin)
        ->sendRequest(
            req,
            [callback = std::move(done)](::drogon::ReqResult result,
                                         const ::drogon::HttpResponsePtr &res) {
                if (result != ::drogon::ReqResult::Ok || !res) {
                    callback({.status = 0,
                              .body = {},
                              .error = std::string{::drogon::to_string_view(result)}});
                    return;
                }
                callback({.status = static_cast<int>(res->statusCode()),
                          .body = std::string{res->body()},
                          .error = std::nullopt});
            },
            seconds);
}

} // namespace inertia::drogon
