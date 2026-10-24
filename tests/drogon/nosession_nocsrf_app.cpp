#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// No sessions and CSRF disabled: the adapter must leave plain routes alone.
namespace inertia::drogon_test {

void configureApp() {
    Config config;
    config.version = [] { return std::string("v1"); };
    inertia::drogon::CsrfOptions csrf;
    csrf.enabled = false;
    inertia::drogon::install(std::make_shared<Inertia>(std::move(config)), std::move(csrf));

    using Callback = std::function<void(const ::drogon::HttpResponsePtr &)>;
    ::drogon::app().registerHandler("/ping",
                                    [](const ::drogon::HttpRequestPtr &, Callback &&done) {
                                        auto res = ::drogon::HttpResponse::newHttpResponse();
                                        res->setBody("pong");
                                        std::move(done)(res);
                                    },
                                    {::drogon::Get, ::drogon::Post});
}

} // namespace inertia::drogon_test
