#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>

#include <drogon/HttpAppFramework.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

// A server whose application forgot app().enableSession().
namespace inertia::drogon_test {

void configureApp() {
    Config config;
    config.version = [] { return std::string("v1"); };
    inertia::drogon::install(std::make_shared<Inertia>(std::move(config)));

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
