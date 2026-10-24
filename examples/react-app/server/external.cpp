#include "routes.hpp"
#include "support.hpp"

#include <inertia/drogon/drogon.hpp>
#include <inertia/props.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace example {

// External: location() sends an Inertia request a 409 + X-Inertia-Location so the client does a
// full-page visit; any other request just gets a 302. The target is a plain (non-Inertia) page
// served by this app, so the demo needs no network.
void registerExternal() {
    registerPage({.path = "/external", .component = "External"},
                 [](const ::drogon::HttpRequestPtr &) { return inertia::Props{}; });

    ::drogon::app().registerHandler("/external/go",
                                    [](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                                        reply(std::move(done),
                                              inertia::drogon::location(req, "/external/landing"));
                                    },
                                    {::drogon::Get});

    ::drogon::app().registerHandler(
        "/external/landing",
        [](const ::drogon::HttpRequestPtr &, Callback &&done) {
            auto res = ::drogon::HttpResponse::newHttpResponse();
            res->setContentTypeCode(::drogon::CT_TEXT_HTML);
            res->setBody("<!doctype html><title>Landing</title><h1>Landed outside Inertia</h1>");
            std::move(done)(res);
        },
        {::drogon::Get});
}

} // namespace example
