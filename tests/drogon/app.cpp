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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inertia::drogon_test {

void configureApp() {
    auto &app = ::drogon::app();
    app.enableSession(std::chrono::minutes(10));

    Config config;
    config.version = [] { return std::string("v1"); };
    // /echo stands in for the SSR server (called by DrogonHttpClient, which sends no CSRF token).
    inertia::drogon::install(std::make_shared<Inertia>(std::move(config)),
                             {.enabled = true, .exceptPaths = {"/echo"}});

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
    registerPage("/throws", "Boom", []() -> Props {
        return {{"ok", 1}, {"boom", []() -> Json { throw std::runtime_error("prop failed"); }}};
    });
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
