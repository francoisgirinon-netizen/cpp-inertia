#include "context.hpp"
#include "routes.hpp"

#include <inertia/inertia.hpp>
#include <inertia/props.hpp>
#include <inertia/session.hpp>

namespace example {

// History: this page opts into history encryption (the client encrypts it in the browser's
// history state); POST /history/clear asks the client to discard the encrypted entries, which
// is what a logout does.
void registerHistory(Router &router) {
    router.get("/history", [](Context &) -> Outcome {
        inertia::RenderOptions options;
        options.encryptHistory = true;
        return Page{.component = "History",
                    .props = inertia::Props{{"secret", "s3cret-token-42"}},
                    .options = options};
    });

    router.post("/history/clear", [](Context &ctx) -> Outcome {
        auto &session = ctx.session();
        inertia::clearHistory(session);
        inertia::flash(session, "success", "History cleared.");
        return inertia::redirect("/history");
    });
}

} // namespace example
