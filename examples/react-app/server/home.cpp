#include "context.hpp"
#include "routes.hpp"

#include <inertia/props.hpp>

#include <string>
#include <utility>

namespace example {

// Home: shared props (registered in app.cpp) and flash messages ride along automatically.
void registerHome(Router &router) {
    registerPage(router, {.path = "/", .component = "Home"},
                 [](Context &) { return inertia::Props{{"greeting", "Hello from C++"}}; });
}

} // namespace example
