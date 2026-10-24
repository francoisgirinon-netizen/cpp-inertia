#include <inertia/ssr.hpp>

#include <inertia/html.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/vite.hpp>

#include <algorithm>
#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

bool parseHealth(const HttpResult &reply) {
    if (reply.error || reply.status < 200 || reply.status >= 300) {
        return false;
    }
    const Json parsed = Json::parse(reply.body, nullptr, false);
    return parsed.is_object() && parsed.contains("status") && parsed["status"] == "OK";
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

namespace {

// Runs `call` with a reply callback that delivers parse(reply) to `done` at most once. When the
// client itself throws before answering, `done` receives `fallback` instead. An exception thrown
// by `done` is never turned into a fallback: it propagates to the caller.
template <typename R, typename Call, typename Parse>
void dispatch(Call call, Parse parse, std::function<void(R)> done, R fallback) {
    struct State {
        std::function<void(R)> done;
        std::atomic<bool> fired{false};
        std::exception_ptr doneError;
    };
    auto state = std::make_shared<State>();
    state->done = std::move(done);
    auto onReply = [state, parse, fallback](const HttpResult &reply) {
        if (state->fired.exchange(true)) {
            return;
        }
        R value = [&]() -> R {
            try {
                return parse(reply);
            } catch (...) {
                return fallback; // a malformed reply keeps the fallback
            }
        }();
        try {
            state->done(std::move(value));
        } catch (...) {
            state->doneError = std::current_exception();
            throw;
        }
    };
    try {
        call(std::move(onReply));
    } catch (...) {
        if (state->doneError) {
            std::rethrow_exception(state->doneError);
        }
        if (!state->fired.exchange(true)) {
            state->done(std::move(fallback));
        }
    }
}

} // namespace

void SsrGateway::render(const Json &page,
                        std::function<void(std::optional<SsrResult>)> done) const {
    if (!http_) {
        done(std::nullopt);
        return;
    }
    dispatch<std::optional<SsrResult>>(
        [&](std::function<void(HttpResult)> cb) {
            http_->post(renderUrl(), dumpJson(page), cfg_.timeout, std::move(cb));
        },
        parseReply, std::move(done), std::nullopt);
}

void SsrGateway::health(std::function<void(bool)> done) const {
    if (!http_) {
        done(false);
        return;
    }
    dispatch<bool>(
        [&](std::function<void(HttpResult)> cb) {
            http_->get(baseUrl() + "/health", cfg_.timeout, std::move(cb));
        },
        parseHealth, std::move(done), false);
}

} // namespace inertia
