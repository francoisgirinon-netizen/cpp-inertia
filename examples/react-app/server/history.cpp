#include "routes.hpp"
#include "support.hpp"

#include <inertia/props.hpp>
#include <inertia/session.hpp>

namespace example {

// History: this page opts into history encryption (the client encrypts it in the browser's
// history state); POST /history/clear asks the client to discard the encrypted entries, which
// is what a logout does.
void registerHistory() {
    ::drogon::app().registerHandler(
        "/history",
        [](const ::drogon::HttpRequestPtr &req, Callback &&done) {
            inertia::RenderOptions options;
            options.encryptHistory = true;
            inertia::drogon::render(req, "History", inertia::Props{{"secret", "s3cret-token-42"}},
                                    std::move(done), options);
        },
        {::drogon::Get});

    ::drogon::app().registerHandler("/history/clear",
                                    [](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                                        auto session = sessionOf(req);
                                        inertia::clearHistory(session);
                                        inertia::flash(session, "success", "History cleared.");
                                        reply(std::move(done), inertia::redirect("/history"));
                                    },
                                    {::drogon::Post});
}

} // namespace example
