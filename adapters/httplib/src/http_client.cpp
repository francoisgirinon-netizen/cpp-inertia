#include <inertia/httplib/httplib.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace inertia::httplib {

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
    const auto pathStart = url.find_first_of("/?#", schemeEnd + 3);
    const auto origin = url.substr(0, pathStart);
    if (origin.size() <= schemeEnd + 3 || origin.find_first_of(" @") != std::string_view::npos) {
        return std::nullopt; // no host, whitespace, or userinfo (which cpp-httplib would misparse)
    }
    std::string path = "/";
    if (pathStart != std::string_view::npos) {
        auto rest = url.substr(pathStart, url.find('#', pathStart) - pathStart);
        path = (rest.empty() || rest.front() == '?' ? "/" : "") + std::string{rest};
    }
    return SplitUrl{.origin = std::string{origin}, .path = std::move(path)};
}

HttpResult failure(std::string message) {
    return {.status = 0, .body = {}, .error = std::move(message)};
}

// Never throws: every failure becomes an HttpResult with an error.
HttpResult perform(std::string_view url, const std::string *jsonBody,
                   std::chrono::milliseconds timeout) {
    try {
        const auto parts = splitUrl(url);
        if (!parts) {
            return failure("invalid URL: " + std::string{url});
        }
        if (!parts->origin.starts_with("http://")) {
            return failure("unsupported URL scheme (only http:// is available): " +
                           std::string{url});
        }
        ::httplib::Client client{parts->origin};
        if (!client.is_valid()) {
            return failure("invalid URL: " + std::string{url});
        }
        const auto bound = std::max(timeout, std::chrono::milliseconds{1});
        client.set_connection_timeout(bound);
        client.set_read_timeout(bound);
        client.set_write_timeout(bound);
        client.set_max_timeout(bound);

        const auto res = jsonBody != nullptr
                             ? client.Post(parts->path, *jsonBody, "application/json")
                             : client.Get(parts->path);
        if (!res) {
            return failure(::httplib::to_string(res.error()));
        }
        return {.status = res->status, .body = res->body, .error = std::nullopt};
    } catch (const std::exception &error) {
        return failure(error.what());
    } catch (...) {
        return failure("unknown error");
    }
}

} // namespace

void HttplibHttpClient::post(std::string url, std::string jsonBody,
                             std::chrono::milliseconds timeout,
                             std::function<void(HttpResult)> done) {
    done(perform(url, &jsonBody, timeout));
}

void HttplibHttpClient::get(std::string url, std::chrono::milliseconds timeout,
                            std::function<void(HttpResult)> done) {
    done(perform(url, nullptr, timeout));
}

} // namespace inertia::httplib
