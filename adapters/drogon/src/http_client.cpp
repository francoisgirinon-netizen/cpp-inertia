#include <inertia/drogon/drogon.hpp>

#include <trantor/net/EventLoop.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
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

struct DrogonHttpClient::Pool {
    using Clock = std::chrono::steady_clock;
    struct Entry {
        ::drogon::HttpClientPtr client;
        std::optional<Clock::time_point> lastEviction;
        std::chrono::milliseconds window{0};
    };

    std::mutex mutex;
    std::map<std::string, Entry> entries; // guarded by mutex

    ::drogon::HttpClientPtr clientFor(const std::string &origin, trantor::EventLoop *loop) {
        const std::scoped_lock lock{mutex};
        auto &entry = entries[origin];
        if (!entry.client) {
            entry.client = ::drogon::HttpClient::newHttpClient(origin, loop);
        }
        return entry.client;
    }

    // Drops `used` from the pool after it timed out, so the next request gets a fresh client.
    // Does nothing when the pool already moved on to another client, or when this origin was
    // evicted less than the window of that earlier eviction ago (the timeout of the request that
    // caused it): a persistently hung server must not cost a connection per request.
    // Returns the evicted client, if any. The caller runs inside that client's own timeout
    // callback, so it must not let the last reference die there: see release().
    [[nodiscard]] ::drogon::HttpClientPtr evict(const std::string &origin,
                                                const std::weak_ptr<::drogon::HttpClient> &used,
                                                std::chrono::milliseconds timeout) {
        const std::scoped_lock lock{mutex};
        const auto it = entries.find(origin);
        if (it == entries.end() || !it->second.client || it->second.client != used.lock()) {
            return nullptr;
        }
        const auto now = Clock::now();
        if (it->second.lastEviction && now - *it->second.lastEviction < it->second.window) {
            return nullptr;
        }
        it->second.lastEviction = now;
        it->second.window = timeout;
        return std::move(it->second.client);
    }

    // Releases an evicted client after the current callback has returned. Destroying a
    // HttpClient from inside one of its own callbacks (on its event loop) would tear down the
    // object while Drogon is still running its code; queueing the release on the loop defers
    // it until that code has unwound, whatever Drogon does with the client internally.
    static void release(::drogon::HttpClientPtr evicted) {
        if (!evicted) {
            return;
        }
        if (auto *loop = trantor::EventLoop::getEventLoopOfCurrentThread()) {
            loop->queueInLoop([holder = std::move(evicted)] {});
        }
        // Not on a loop thread (cannot happen for a Drogon callback): the pointer is released here.
    }
};

DrogonHttpClient::DrogonHttpClient(trantor::EventLoop *loop)
    : loop_(loop), pool_(std::make_shared<Pool>()) {}

void DrogonHttpClient::post(std::string url, std::string jsonBody,
                            std::chrono::milliseconds timeout,
                            std::function<void(HttpResult)> done) {
    send(::drogon::Post, url, std::move(jsonBody), timeout, std::move(done));
}

void DrogonHttpClient::get(std::string url, std::chrono::milliseconds timeout,
                           std::function<void(HttpResult)> done) {
    send(::drogon::Get, url, {}, timeout, std::move(done));
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
    const auto client = pool_->clientFor(parts->origin, loop_);
    // A weak pointer: the callback must not keep its own client alive.
    client->sendRequest(
        req,
        [callback = std::move(done), pool = pool_, origin = parts->origin,
         used = std::weak_ptr<::drogon::HttpClient>{client},
         timeout](::drogon::ReqResult result, const ::drogon::HttpResponsePtr &res) {
            if (result == ::drogon::ReqResult::Timeout) {
                Pool::release(pool->evict(origin, used, timeout));
            }
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
