#include "routes.hpp"
#include "support.hpp"

#include <inertia/props.hpp>
#include <inertia/session.hpp>

namespace example {

// Fragment: browsers drop the #fragment of a redirect that an XHR follows. preserveFragment()
// tells the client to keep it, so the visit lands on #section.
void registerFragment() {
    registerPage({.path = "/fragment", .component = "Fragment"},
                 [](const ::drogon::HttpRequestPtr &) { return inertia::Props{}; });

    ::drogon::app().registerHandler("/fragment/jump",
                                    [](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                                        auto session = sessionOf(req);
                                        inertia::preserveFragment(session);
                                        reply(std::move(done),
                                              inertia::redirect("/fragment#section"));
                                    },
                                    {::drogon::Post});
}

} // namespace example
