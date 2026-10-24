#pragma once

#include <inertia/inertia.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace inertia::conformance {

using Handler = std::function<void(const RequestView &, SessionStore &, const Inertia &,
                                   std::function<void(Response)>)>;

struct Route {
    Route(std::string verb, std::string pathOnly, Handler fn)
        : method(std::move(verb)), path(std::move(pathOnly)), handler(std::move(fn)) {}

    std::string method;
    std::string path; // without query string
    Handler handler;
};

// Version "v1", SSR off.
[[nodiscard]] Config conformanceConfig();

// An Inertia built from conformanceConfig() that shares "app": "cpp-inertia".
[[nodiscard]] Inertia makeConformanceApp();

// One route per scenario family; see routes.cpp.
[[nodiscard]] std::vector<Route> conformanceRoutes();

} // namespace inertia::conformance
