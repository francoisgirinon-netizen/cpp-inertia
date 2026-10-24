#include "convert_internal.hpp"

#include <inertia/attributes.hpp>
#include <inertia/drogon/drogon.hpp>
#include <inertia/headers.hpp>
#include <inertia/inertia.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>

#include <drogon/Cookie.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/Session.h>
#include <drogon/drogon_callbacks.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia::drogon {

namespace {

std::atomic<std::shared_ptr<const Inertia>> &installed() {
    static std::atomic<std::shared_ptr<const Inertia>> instance;
    return instance;
}

// CsrfOptions::trustForwardedHeaders of the installed adapter, for the request views that
// render() and location() build themselves.
std::atomic<bool> &trustForwarded() {
    static std::atomic<bool> value{false};
    return value;
}

std::shared_ptr<const Inertia> requireInstalled() {
    auto instance = installed().load();
    if (!instance) {
        throw std::logic_error("inertia::drogon::install() has not been called");
    }
    return instance;
}

constexpr std::string_view kSessionsDisabled =
    "inertia: sessions are disabled: call app().enableSession() before app().run()";

::drogon::HttpResponsePtr serverError(std::string_view message = "Internal Server Error") {
    auto res = ::drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(::drogon::k500InternalServerError);
    res->setBody(std::string{message});
    return res;
}

constexpr std::string_view kTokenKey = "inertia.csrf_token";
constexpr std::size_t kTokenBytes = 32;

std::string newToken() {
    std::array<unsigned char, kTokenBytes> bytes{};
    if (!::drogon::utils::secureRandomBytes(bytes.data(), bytes.size())) {
        throw std::runtime_error("no secure random source for the CSRF token");
    }
    return ::drogon::utils::binaryStringToHex(bytes.data(), bytes.size());
}

// The session's token, created on first use. modify() runs under the session lock, so two
// concurrent first requests of one session agree on a single token.
std::string tokenOf(const ::drogon::SessionPtr &session) {
    std::string token;
    session->modify<std::string>(std::string{kTokenKey}, [&token](std::string &stored) {
        if (stored.empty()) {
            stored = newToken();
        }
        token = stored;
    });
    return token;
}

// Compares without exiting early on the first differing byte.
bool constantTimeEquals(std::string_view a, std::string_view b) INERTIA_NONALLOCATING {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned>(static_cast<unsigned char>(a[i])) ^
                static_cast<unsigned>(static_cast<unsigned char>(b[i]));
    }
    return diff == 0;
}

bool isSafeMethod(std::string_view method) INERTIA_NONALLOCATING {
    return method == "GET" || method == "HEAD" || method == "OPTIONS";
}

bool isExempt(const CsrfOptions &csrf, const std::string &path) {
    return std::ranges::any_of(csrf.exceptPaths, [&](const std::string &rule) {
        if (!rule.empty() && rule.back() == '*') {
            return path.starts_with(std::string_view{rule}.substr(0, rule.size() - 1));
        }
        return path == rule;
    });
}

bool csrfMatches(const CsrfOptions &csrf, const ::drogon::HttpRequestPtr &req) {
    const auto &session = req->session();
    if (!session) {
        return false;
    }
    const auto sent = req->getHeader(csrf.header);
    const auto expected = session->getOptional<std::string>(std::string{kTokenKey});
    return !sent.empty() && expected && constantTimeEquals(sent, *expected);
}

// Sets the token cookie unless the client already presents the current token.
void attachCsrfCookie(const CsrfOptions &csrf, const ::drogon::HttpRequestPtr &req,
                      const ::drogon::HttpResponsePtr &res) {
    const auto &session = req->session();
    if (!session) {
        return;
    }
    const auto token = tokenOf(session);
    if (constantTimeEquals(req->getCookie(csrf.cookie), token)) {
        return;
    }
    ::drogon::Cookie cookie{csrf.cookie, token};
    cookie.setPath("/");
    cookie.setHttpOnly(false); // scripts must read it to echo it back in the header
    cookie.setSameSite(::drogon::Cookie::SameSite::kLax);
    cookie.setSecure(DrogonRequest{req, csrf.trustForwardedHeaders}.isSecure());
    res->addCookie(std::move(cookie));
}

::drogon::HttpResponsePtr pageExpired() {
    auto res = ::drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(static_cast<::drogon::HttpStatusCode>(419)); // Laravel's "page expired"
    res->setBody("CSRF token mismatch");
    return res;
}

bool isManagedElsewhere(std::string_view name) {
    // Content-Type and cookies are not plain headers in Drogon's response model.
    return headers::iequals(name, "Content-Type") || headers::iequals(name, "Set-Cookie");
}

// Runs Inertia::finalize on the response and writes its edits (status, body, headers) back.
void finalizeResponse(const Inertia &inertia, const ::drogon::HttpRequestPtr &req,
                      const ::drogon::HttpResponsePtr &res) {
    Response meta = detail::responseHead(res);
    const int status = meta.status;
    // Inertia::finalize only asks whether the body is empty, and may clear it. A one-byte stand-in
    // answers that without copying a body that can be hundreds of kilobytes.
    const bool hadBody = !res->body().empty();
    if (hadBody) {
        meta.body = " ";
    }
    const Response before{.status = status, .headers = meta.headers, .body = {}};

    inertia.finalize(DrogonRequest{req, trustForwarded().load()}, meta);

    if (meta.status != status) {
        res->setStatusCode(static_cast<::drogon::HttpStatusCode>(meta.status));
    }
    if (hadBody && meta.body.empty()) {
        res->setBody(std::string{});
    }
    for (const auto &[name, value] : before.headers) {
        if (!isManagedElsewhere(name) && !meta.header(name)) {
            res->removeHeader(name);
        }
    }
    for (const auto &[name, value] : meta.headers) {
        if (!isManagedElsewhere(name) && before.header(name) != value) {
            res->removeHeader(name);
            res->addHeader(name, value);
        }
    }
}

} // namespace

void install(const std::shared_ptr<const Inertia> &inertia, CsrfOptions csrf) {
    if (!inertia) {
        throw std::invalid_argument("inertia::drogon::install() needs an Inertia instance");
    }
    // The trust flag is set before the instance is published, so render()/location() never see
    // an installed adapter with a stale flag; a rejected second call puts the old value back.
    const bool previousTrust = trustForwarded().exchange(csrf.trustForwardedHeaders);
    std::shared_ptr<const Inertia> none;
    if (!installed().compare_exchange_strong(none, inertia)) {
        trustForwarded().store(previousTrust);
        throw std::logic_error("inertia::drogon::install() was already called: the adapter can be "
                               "installed only once per process");
    }
    auto &app = ::drogon::app();
    const auto options = std::make_shared<const CsrfOptions>(std::move(csrf));

    // Version, CSRF, and the post-handling steps for a response the pre-handling advice produced
    // itself (Drogon does not run post-handling advices for those).
    const auto afterHandling = [inertia, options](const ::drogon::HttpRequestPtr &req,
                                                  const ::drogon::HttpResponsePtr &res) {
        if (options->enabled) {
            attachCsrfCookie(*options, req, res);
        }
        finalizeResponse(*inertia, req, res);
    };

    app.registerPreHandlingAdvice(
        [inertia, options, afterHandling](const ::drogon::HttpRequestPtr &req,
                                          ::drogon::AdviceCallback &&stop,
                                          ::drogon::AdviceChainCallback &&next) {
            ::drogon::HttpResponsePtr early;
            try {
                const DrogonRequest view{req, options->trustForwardedHeaders};
                if (auto versionMismatch = inertia->handleVersion(view)) {
                    early = toDrogon(*versionMismatch);
                } else if (options->enabled && !isSafeMethod(view.method()) &&
                           !isExempt(*options, req->path())) {
                    if (!req->session()) {
                        // Drogon exposes no way to ask whether sessions are enabled, so a
                        // request without one is the earliest sign. Say so plainly instead of
                        // a 419. Only the CSRF check needs the session: other routes are left
                        // alone.
                        static std::atomic<bool> reported{false};
                        if (!reported.exchange(true)) {
                            LOG_ERROR << kSessionsDisabled;
                        }
                        early = serverError(kSessionsDisabled);
                    } else if (!csrfMatches(*options, req)) {
                        early = pageExpired();
                    }
                }
                if (early) {
                    afterHandling(req, early);
                }
            } catch (const std::exception &error) {
                LOG_ERROR << "inertia: pre-handling failed: " << error.what();
                early = serverError();
            }
            if (early) {
                std::move(stop)(early);
                return;
            }
            std::move(next)();
        });

    app.registerPostHandlingAdvice(
        [afterHandling](const ::drogon::HttpRequestPtr &req, const ::drogon::HttpResponsePtr &res) {
            try {
                afterHandling(req, res);
            } catch (const std::exception &error) {
                LOG_ERROR << "inertia: post-handling failed: " << error.what();
            }
        });
}

void render(const ::drogon::HttpRequestPtr &req, std::string component, Props props,
            std::function<void(const ::drogon::HttpResponsePtr &)> &&callback, RenderOptions opts) {
    // The callback must fire exactly once: either Inertia::render completes (possibly later,
    // on another thread, with SSR) or an exception turns into a 500 - never both.
    struct State {
        std::function<void(const ::drogon::HttpResponsePtr &)> callback;
        std::atomic<bool> answered{false};
        void answer(const ::drogon::HttpResponsePtr &res) {
            if (!answered.exchange(true)) {
                callback(res);
            }
        }
    };
    auto state = std::make_shared<State>();
    state->callback = std::move(callback);
    try {
        const auto session = req->session();
        if (!session) {
            LOG_ERROR << "inertia: render failed: " << kSessionsDisabled;
            state->answer(serverError(kSessionsDisabled));
            return;
        }
        // The session store is only used until Inertia::render returns or its SSR continuation
        // runs; both keep the request alive through the captured pointer below.
        auto store = std::make_shared<DrogonSession>(session);
        auto view = std::make_shared<DrogonRequest>(req, trustForwarded().load());
        requireInstalled()->render(
            *view, *store, std::move(component), std::move(props),
            [state, store, view](Response res) { state->answer(toDrogon(std::move(res))); }, opts);
    } catch (const std::exception &error) {
        LOG_ERROR << "inertia: render failed: " << error.what();
        state->answer(serverError());
    } catch (...) {
        LOG_ERROR << "inertia: render failed with a non-standard exception";
        state->answer(serverError());
    }
}

Response location(const ::drogon::HttpRequestPtr &req, std::string_view url) {
    return requireInstalled()->location(DrogonRequest{req, trustForwarded().load()}, url);
}

} // namespace inertia::drogon
