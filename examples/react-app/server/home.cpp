#include "routes.hpp"
#include "support.hpp"

#include <inertia/props.hpp>

#include <drogon/HttpRequest.h>

#include <string>
#include <utility>

namespace example {

// Home: shared props (registered in main.cpp) and flash messages ride along automatically.
void registerHome() {
    registerPage({.path = "/", .component = "Home"}, [](const ::drogon::HttpRequestPtr &) {
        return inertia::Props{{"greeting", "Hello from C++"}};
    });
}

} // namespace example
