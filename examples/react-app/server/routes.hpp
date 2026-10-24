#pragma once

#include "context.hpp"

// One function per feature group; each registers its routes on the router.
namespace example {

void registerHome(Router &router);
void registerUsers(Router &router);
void registerDashboard(Router &router);
void registerFeed(Router &router);
void registerPosts(Router &router);
void registerHistory(Router &router);
void registerExternal(Router &router);
void registerFragment(Router &router);
void registerProps(Router &router);
void registerLogin(Router &router);
void registerVariants(Router &router);

// Every route of the example.
void registerRoutes(Router &router);

} // namespace example
