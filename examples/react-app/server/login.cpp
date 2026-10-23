#include "routes.hpp"
#include "support.hpp"

#include <inertia/props.hpp>
#include <inertia/session.hpp>

#include <string>
#include <vector>

namespace example {

// Login: validation errors in a named error bag. The client sends `errorBag: 'login'`
// (X-Inertia-Error-Bag), so the page's `errors` prop holds only that bag's messages, and several
// forms on one page do not show each other's errors.
void registerLogin() {
    registerPage({.path = "/login", .component = "Login"},
                 [](const ::drogon::HttpRequestPtr &) { return inertia::Props{}; });

    ::drogon::app().registerHandler(
        "/login",
        [](const ::drogon::HttpRequestPtr &req, Callback &&done) {
            const auto body = jsonBody(req);
            const auto password = body.value("password", std::string{});
            auto session = sessionOf(req);
            if (password != "secret") {
                inertia::withErrors(
                    session, {{"password", std::vector<std::string>{"The password is wrong."}}},
                    "login");
                return reply(std::move(done), inertia::back(inertia::drogon::DrogonRequest{req}));
            }
            inertia::flash(session, "success", "Logged in.");
            reply(std::move(done), inertia::redirect("/"));
        },
        {::drogon::Post});
}

} // namespace example
