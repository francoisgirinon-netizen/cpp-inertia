#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <trantor/net/EventLoop.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inertia::drogon_test {

namespace {

// Set by GET /break-version?on=1 (cleared by on=0): while set, the version callable throws.
std::atomic<bool> &versionBroken() {
    static std::atomic<bool> value{false};
    return value;
}

} // namespace

namespace {

constexpr std::size_t kBigBodySize = 65536;

std::atomic<const char *> &bodyBefore() {
    static std::atomic<const char *> pointer{nullptr};
    return pointer;
}

std::atomic<const char *> &bodyAfter() {
    static std::atomic<const char *> pointer{nullptr};
    return pointer;
}

} // namespace

void configureApp() {
    auto &app = ::drogon::app();
    app.enableSession(std::chrono::minutes(10));

    Config config;
    config.version = [] {
        if (versionBroken()) {
            throw std::runtime_error("version unavailable");
        }
        return std::string("v1");
    };
    // /echo stands in for the SSR server (called by DrogonHttpClient, which sends no CSRF token).
    // /hook/* covers a wildcard exception rule.
    inertia::drogon::install(std::make_shared<Inertia>(std::move(config)),
                             {.enabled = true, .exceptPaths = {"/echo", "/hook/*"}});

    using Callback = std::function<void(const ::drogon::HttpResponsePtr &)>;
    const auto registerPage = [&app](const std::string &path, const char *name,
                                     std::function<Props()> propsFn) {
        app.registerHandler(path,
                            [component = std::string{name}, makeProps = std::move(propsFn)](
                                const ::drogon::HttpRequestPtr &req, Callback &&done) {
                                inertia::drogon::render(req, component, makeProps(),
                                                        std::move(done));
                            },
                            {::drogon::Get});
    };
    const auto text = [](std::string body) {
        auto res = ::drogon::HttpResponse::newHttpResponse();
        res->setBody(std::move(body));
        return res;
    };

    registerPage("/page", "Page", [] { return Props{{"title", "Home"}, {"count", 1}}; });
    registerPage("/throws", "Boom", []() -> Props { return {{"ok", 1}, {"boom", failingProp}}; });
    // A value that is not a std::exception: the adapter must still answer 500.
    registerPage("/throws-value", "Boom", []() -> Props { return {{"boom", failingPropNonStd}}; });
    // A 302 to a URL with a fragment: Inertia asks the client to visit it itself.
    app.registerHandler("/fragment",
                        [](const ::drogon::HttpRequestPtr &, Callback &&done) {
                            auto res = ::drogon::HttpResponse::newHttpResponse();
                            res->setStatusCode(::drogon::k302Found);
                            res->addHeader("Location", "/page#top");
                            res->setBody("moved");
                            std::move(done)(res);
                        },
                        {::drogon::Get});
    app.registerHandler("/break-version",
                        [text](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            versionBroken() = req->getParameter("on") == "1";
                            std::move(done)(text("broken"));
                        },
                        {::drogon::Get});
    app.registerHandler("/hook/ping",
                        [text](const ::drogon::HttpRequestPtr &, Callback &&done) {
                            std::move(done)(text("hooked"));
                        },
                        {::drogon::Post});
    app.registerHandler("/external",
                        [](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            std::move(done)(inertia::drogon::toDrogon(
                                inertia::drogon::location(req, "https://example.com/away")));
                        },
                        {::drogon::Get});
    app.registerHandler("/vary",
                        [text](const ::drogon::HttpRequestPtr &, Callback &&done) {
                            auto res = text("ok");
                            res->addHeader("Vary", "Accept-Encoding");
                            std::move(done)(res);
                        },
                        {::drogon::Get});

    // PUT /session/put?v=<json> stores the JSON under "k".
    app.registerHandler("/session/put",
                        [text](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            inertia::drogon::DrogonSession session{req->session()};
                            session.put("k", Json::parse(req->getParameter("v")));
                            std::move(done)(text("stored"));
                        },
                        {::drogon::Get});
    app.registerHandler("/session/get",
                        [text](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            const inertia::drogon::DrogonSession session{req->session()};
                            const auto value = session.get("k");
                            std::move(done)(text(value ? value->dump() : "missing"));
                        },
                        {::drogon::Get});
    app.registerHandler("/session/pull",
                        [text](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            inertia::drogon::DrogonSession session{req->session()};
                            const auto value = session.pull("k");
                            std::move(done)(text(value ? value->dump() : "missing"));
                        },
                        {::drogon::Get});

    // POST /echo answers with the request body and the Content-Type it arrived with.
    app.registerHandler("/echo",
                        [text](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            auto res = text(std::string{req->body()});
                            res->addHeader("X-Seen-Content-Type", req->getHeader("content-type"));
                            std::move(done)(res);
                        },
                        {::drogon::Post});
    app.registerHandler(
        "/submit",
        [text](const ::drogon::HttpRequestPtr &, Callback &&done) {
            std::move(done)(text("submitted"));
        },
        {::drogon::Post, ::drogon::Put, ::drogon::Patch, ::drogon::Delete, ::drogon::Get});
    // GET /big answers 200 with a large body. Finalize must leave it alone, not even copy it: the
    // handler and an advice registered after the adapter's compare the body buffers.
    app.registerHandler("/big",
                        [text](const ::drogon::HttpRequestPtr &, Callback &&done) {
                            auto res = text(std::string(kBigBodySize, 'b'));
                            bodyBefore().store(std::to_address(res->body().begin()));
                            std::move(done)(res);
                        },
                        {::drogon::Get});
    app.registerPostHandlingAdvice(
        [](const ::drogon::HttpRequestPtr &req, const ::drogon::HttpResponsePtr &res) {
            if (req->path() == "/big") {
                bodyAfter().store(std::to_address(res->body().begin()));
            }
        });
    app.registerHandler("/big/same-buffer",
                        [text](const ::drogon::HttpRequestPtr &, Callback &&done) {
                            const bool same = bodyBefore().load() != nullptr &&
                                              bodyBefore().load() == bodyAfter().load();
                            std::move(done)(text(same ? "same" : "different"));
                        },
                        {::drogon::Get});
    app.registerHandler("/empty",
                        [](const ::drogon::HttpRequestPtr &, Callback &&done) {
                            std::move(done)(::drogon::HttpResponse::newHttpResponse());
                        },
                        {::drogon::Get});
    app.registerHandler("/ping",
                        [text](const ::drogon::HttpRequestPtr &, Callback &&done) {
                            std::move(done)(text("pong"));
                        },
                        {::drogon::Get});
    // GET /slow answers after kSlowDelay.
    app.registerHandler("/slow",
                        [text](const ::drogon::HttpRequestPtr &, Callback &&done) {
                            auto reply = std::make_shared<Callback>(std::move(done));
                            ::drogon::app().getLoop()->runAfter(
                                std::chrono::duration<double>(kSlowDelay).count(),
                                [reply, text] { (*reply)(text("late")); });
                        },
                        {::drogon::Get});
}

} // namespace inertia::drogon_test
