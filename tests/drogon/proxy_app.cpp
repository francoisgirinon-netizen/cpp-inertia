#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>
#include <inertia/props.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// A server behind a trusted TLS-terminating proxy.
namespace inertia::drogon_test {

void configureApp() {
    ::drogon::app().enableSession(std::chrono::minutes(10));

    Config config;
    config.version = [] { return std::string("v1"); };
    inertia::drogon::CsrfOptions options;
    options.trustForwardedHeaders = true;
    inertia::drogon::install(std::make_shared<Inertia>(std::move(config)), std::move(options));

    using Callback = std::function<void(const ::drogon::HttpResponsePtr &)>;
    ::drogon::app().registerHandler("/ping",
                                    [](const ::drogon::HttpRequestPtr &, Callback &&done) {
                                        auto res = ::drogon::HttpResponse::newHttpResponse();
                                        res->setBody("pong");
                                        std::move(done)(res);
                                    },
                                    {::drogon::Get});
    // Stale asset version: the 409 carries an absolute X-Inertia-Location built from fullUrl().
    ::drogon::app().registerHandler("/page",
                                    [](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                                        inertia::drogon::render(req, "Page", {}, std::move(done));
                                    },
                                    {::drogon::Get});
}

} // namespace inertia::drogon_test
