#include "context.hpp"
#include "routes.hpp"

#include <inertia/props.hpp>
#include <inertia/session.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace example {

// Login: validation errors in a named error bag. The client sends `errorBag: 'login'`
// (X-Inertia-Error-Bag), so the page's `errors` prop holds only that bag's messages, and several
// forms on one page do not show each other's errors.
void registerLogin(Router &router) {
    registerPage(router, {.path = "/login", .component = "Login"},
                 [](Context &) { return inertia::Props{}; });

    router.post("/login", [](Context &ctx) -> Outcome {
        const auto body = jsonBody(ctx);
        const auto password = body.value("password", std::string{});
        auto &session = ctx.session();
        // Demo only: the password is a hard-coded literal. A real app checks a salted hash.
        if (password != "secret") {
            inertia::withErrors(session,
                                {{"password", std::vector<std::string>{"The password is wrong."}}},
                                "login");
            return inertia::back(ctx.request());
        }
        inertia::flash(session, "success", "Logged in.");
        return inertia::redirect("/");
    });
}

} // namespace example
