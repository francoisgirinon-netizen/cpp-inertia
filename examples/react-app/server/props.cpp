#include "routes.hpp"
#include "support.hpp"

#include <inertia/props.hpp>

#include <atomic>
#include <string>

namespace example {

// Props: how partial reloads (`only` / `except`) select props.
void registerProps() {
    // Each counter shows how many times its prop was resolved, i.e. whether the server sent it.
    static std::atomic<int> alwaysResolved{0};
    static std::atomic<int> optionalResolved{0};
    static std::atomic<int> otherResolved{0};

    registerPage({.path = "/props", .component = "Props"}, [](const ::drogon::HttpRequestPtr &) {
        return inertia::Props{
            {"title", "Props"},
            // always(): sent on every response, even a partial reload whose `only` does not list
            // it.
            {"always", inertia::always(inertia::Prop{[] {
                 return inertia::Prop{"always #" + std::to_string(++alwaysResolved)};
             }})},
            // optional(): left out of the first visit and of any reload that does not ask for it
            // by name with `only`.
            {"optional", inertia::optional([] {
                 return inertia::Prop{"optional #" + std::to_string(++optionalResolved)};
             })},
            // A plain closure: sent on full visits, and on partial reloads unless excluded by
            // `except` or left out of `only`.
            {"other", [] { return inertia::Prop{"other #" + std::to_string(++otherResolved)}; }},
        };
    });

    // An Inertia request that gets a 200 with an empty body is turned by Inertia into a redirect
    // back to the page the request came from (so the client never receives an empty page).
    ::drogon::app().registerHandler("/props/empty",
                                    [](const ::drogon::HttpRequestPtr &, Callback &&done) {
                                        std::move(done)(::drogon::HttpResponse::newHttpResponse());
                                    },
                                    {::drogon::Post});
}

} // namespace example
