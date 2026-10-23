#pragma once

// Small helpers shared by the example's route files.

#include <inertia/drogon/drogon.hpp>
#include <inertia/json.hpp>
#include <inertia/session.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>
#include <string>
#include <utility>

namespace example {

using Callback = std::function<void(const ::drogon::HttpResponsePtr &)>;

struct PageRoute {
    std::string path;
    std::string component;
};

// Registers `GET route.path` rendering `route.component` with the props returned by
// makeProps(request).
template <class MakeProps> void registerPage(const PageRoute &route, MakeProps makeProps) {
    ::drogon::app().registerHandler(route.path,
                                    [name = route.component, props = std::move(makeProps)](
                                        const ::drogon::HttpRequestPtr &req, Callback &&done) {
                                        inertia::drogon::render(req, name, props(req),
                                                                std::move(done));
                                    },
                                    {::drogon::Get});
}

// The Inertia view of the request's Drogon session (flash messages, validation errors, ...).
inline inertia::drogon::DrogonSession sessionOf(const ::drogon::HttpRequestPtr &req) {
    return inertia::drogon::DrogonSession{req->session()};
}

// The request's JSON body (what the Inertia client sends for forms), or {} when it is not an
// object.
inline inertia::Json jsonBody(const ::drogon::HttpRequestPtr &req) {
    auto body = inertia::Json::parse(req->body(), nullptr, false);
    return body.is_object() ? body : inertia::Json::object();
}

inline void reply(Callback &&done, const inertia::Response &response) {
    std::move(done)(inertia::drogon::toDrogon(response));
}

} // namespace example
