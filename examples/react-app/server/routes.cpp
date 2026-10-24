#include "routes.hpp"

#include "context.hpp"

#include <inertia/json.hpp>

#include <charconv>
#include <string>

namespace example {

inertia::Json jsonBody(const Context &ctx) {
    auto body = inertia::Json::parse(ctx.body(), nullptr, false);
    return body.is_object() ? body : inertia::Json::object();
}

int intQuery(const Context &ctx, const char *name, int fallback) {
    const auto text = ctx.query(name);
    int value = fallback;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

void registerRoutes(Router &router) {
    registerHome(router);
    registerUsers(router);
    registerDashboard(router);
    registerFeed(router);
    registerPosts(router);
    registerHistory(router);
    registerExternal(router);
    registerFragment(router);
    registerProps(router);
    registerLogin(router);
    registerVariants(router);
}

} // namespace example
