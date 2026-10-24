#include "context.hpp"
#include "routes.hpp"

#include <inertia/props.hpp>
#include <inertia/session.hpp>

namespace example {

// Fragment: browsers drop the #fragment of a redirect that an XHR follows. preserveFragment()
// tells the client to keep it, so the visit lands on #section.
void registerFragment(Router &router) {
    registerPage(router, {.path = "/fragment", .component = "Fragment"},
                 [](Context &) { return inertia::Props{}; });

    router.post("/fragment/jump", [](Context &ctx) -> Outcome {
        inertia::preserveFragment(ctx.session());
        return inertia::redirect("/fragment#section");
    });
}

} // namespace example
