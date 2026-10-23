#include <inertia/html.hpp>
#include <inertia/ssr.hpp>
#include <inertia/vite.hpp>

#include <algorithm>
#include <utility>

namespace inertia {

namespace {

bool matches(const std::string &pattern, std::string_view path) {
    if (!pattern.empty() && pattern.back() == '*') {
        return path.starts_with(std::string_view(pattern).substr(0, pattern.size() - 1));
    }
    return path == pattern;
}

// Parses an SSR reply; nullopt unless it is 2xx with {"head": [string...], "body": string}.
std::optional<SsrResult> parseReply(const HttpResult &reply) {
    if (reply.error || reply.status < 200 || reply.status >= 300) {
        return std::nullopt;
    }
    const Json parsed = Json::parse(reply.body, nullptr, false);
    if (!parsed.is_object() || !parsed.contains("head") || !parsed.contains("body")) {
        return std::nullopt;
    }
    const Json &head = parsed["head"];
    const Json &body = parsed["body"];
    if (!head.is_array() || !body.is_string()) {
        return std::nullopt;
    }
    SsrResult out;
    for (const auto &item : head) {
        if (!item.is_string()) {
            return std::nullopt;
        }
        if (!out.head.empty()) {
            out.head += '\n';
        }
        out.head += item.get<std::string>();
    }
    out.body = body.get<std::string>();
    return out;
}

} // namespace

SsrGateway::SsrGateway(SsrConfig cfg, std::shared_ptr<HttpClient> http,
                       std::shared_ptr<const Vite> vite)
    : cfg_(std::move(cfg)), http_(std::move(http)), vite_(std::move(vite)) {}

bool SsrGateway::shouldRender(std::string_view requestPath) const {
    return cfg_.enabled && http_ &&
           std::ranges::none_of(cfg_.exceptPaths, [requestPath](const std::string &pattern) {
               return matches(pattern, requestPath);
           });
}

std::string SsrGateway::baseUrl() const {
    std::string url = cfg_.url;
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    return url;
}

std::string SsrGateway::renderUrl() const {
    if (vite_) {
        if (auto hot = vite_->hotUrl()) {
            return *hot + "/__inertia_ssr";
        }
    }
    return baseUrl() + "/render";
}

void SsrGateway::render(const Json &page, std::string_view /*requestPath*/,
                        std::function<void(std::optional<SsrResult>)> done) const {
    if (!http_) {
        done(std::nullopt);
        return;
    }
    try {
        http_->post(renderUrl(), dumpJson(page), cfg_.timeout,
                    [cb = std::move(done)](const HttpResult &reply) {
                        std::optional<SsrResult> result;
                        try {
                            result = parseReply(reply);
                        } catch (...) { // NOLINT(bugprone-empty-catch): any failure falls back
                        }
                        cb(std::move(result));
                    });
    } catch (...) { // NOLINT(bugprone-empty-catch): a throwing client must not escape
    }
}

void SsrGateway::health(std::function<void(bool)> done) const {
    if (!http_) {
        done(false);
        return;
    }
    try {
        http_->get(baseUrl() + "/health", cfg_.timeout,
                   [cb = std::move(done)](const HttpResult &reply) {
                       bool ok = false;
                       if (!reply.error && reply.status >= 200 && reply.status < 300) {
                           const Json parsed = Json::parse(reply.body, nullptr, false);
                           ok = parsed.is_object() && parsed.contains("status") &&
                                parsed["status"] == "OK";
                       }
                       cb(ok);
                   });
    } catch (...) { // NOLINT(bugprone-empty-catch): a throwing client must not escape
    }
}

} // namespace inertia
