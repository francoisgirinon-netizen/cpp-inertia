#include <inertia/drogon/drogon.hpp>

#include <inertia/headers.hpp>

#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>

#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace inertia::drogon {

namespace {

std::atomic<std::shared_ptr<const Inertia>> &installed() {
    static std::atomic<std::shared_ptr<const Inertia>> instance;
    return instance;
}

std::shared_ptr<const Inertia> requireInstalled() {
    auto instance = installed().load();
    if (!instance) {
        throw std::logic_error("inertia::drogon::install() has not been called");
    }
    return instance;
}

::drogon::HttpResponsePtr serverError() {
    auto res = ::drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(::drogon::k500InternalServerError);
    res->setBody("Internal Server Error");
    return res;
}

// finalize() only edits headers, so it is given a body-less copy and its result is written back.
void finalizeResponse(const Inertia &inertia, const ::drogon::HttpRequestPtr &req,
                      const ::drogon::HttpResponsePtr &res) {
    Response meta;
    meta.status = static_cast<int>(res->statusCode());
    for (const auto &[name, value] : res->headers()) {
        meta.headers.emplace_back(name, value);
    }
    inertia.finalize(DrogonRequest{req}, meta);
    for (const auto &[name, value] : meta.headers) {
        if (res->getHeader(name) != value) {
            res->removeHeader(name);
            res->addHeader(name, value);
        }
    }
}

} // namespace

void install(const std::shared_ptr<const Inertia> &inertia) {
    installed().store(inertia);
    auto &app = ::drogon::app();

    app.registerPreHandlingAdvice([inertia](const ::drogon::HttpRequestPtr &req,
                                            ::drogon::AdviceCallback &&stop,
                                            ::drogon::AdviceChainCallback &&next) {
        try {
            const DrogonRequest view{req};
            if (auto early = inertia->handleVersion(view)) {
                // Drogon does not run post-handling advices for a response produced by a
                // pre-handling advice, so the short-circuit is finalized here (idempotent).
                inertia->finalize(view, *early);
                std::move(stop)(toDrogon(*early));
                return;
            }
        } catch (const std::exception &error) {
            LOG_ERROR << "inertia: version check failed: " << error.what();
            std::move(stop)(serverError());
            return;
        }
        std::move(next)();
    });

    app.registerPostHandlingAdvice(
        [inertia](const ::drogon::HttpRequestPtr &req, const ::drogon::HttpResponsePtr &res) {
            try {
                finalizeResponse(*inertia, req, res);
            } catch (const std::exception &error) {
                LOG_ERROR << "inertia: finalize failed: " << error.what();
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
            throw std::logic_error("sessions are disabled: call app().enableSession()");
        }
        // The session store is only used until Inertia::render returns or its SSR continuation
        // runs; both keep the request alive through the captured pointer below.
        auto store = std::make_shared<DrogonSession>(session);
        auto view = std::make_shared<DrogonRequest>(req);
        requireInstalled()->render(
            *view, *store, std::move(component), std::move(props),
            [state, store, view](const Response &res) { state->answer(toDrogon(res)); }, opts);
    } catch (const std::exception &error) {
        LOG_ERROR << "inertia: render failed: " << error.what();
        state->answer(serverError());
    } catch (...) {
        LOG_ERROR << "inertia: render failed with a non-standard exception";
        state->answer(serverError());
    }
}

Response location(const ::drogon::HttpRequestPtr &req, std::string_view url) {
    return requireInstalled()->location(DrogonRequest{req}, url);
}

} // namespace inertia::drogon
