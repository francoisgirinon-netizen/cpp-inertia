#include "context.hpp"
#include "routes.hpp"

#include <inertia/props.hpp>
#include <inertia/response.hpp>

#include <string>
#include <utility>
#include <vector>

namespace example {

// External: location() sends an Inertia request a 409 + X-Inertia-Location so the client does a
// full-page visit; any other request just gets a 302. The target is a plain (non-Inertia) page
// served by this app, so the demo needs no network.
void registerExternal(Router &router) {
    registerPage(router, {.path = "/external", .component = "External"},
                 [](Context &) { return inertia::Props{}; });

    router.get("/external/go",
               [](Context &) -> Outcome { return Location{.url = "/external/landing"}; });

    router.get("/external/landing", [](Context &) -> Outcome {
        inertia::Response res;
        res.headers.emplace_back("Content-Type", "text/html; charset=utf-8");
        res.body = "<!doctype html><title>Landing</title><h1>Landed outside Inertia</h1>";
        return res;
    });
}

} // namespace example
