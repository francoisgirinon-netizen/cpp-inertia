#pragma once

#include <inertia/inertia.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/response.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace inertia::conformance {

/// Handler of one conformance route.
///
/// Receives the request view, the session, the application instance and the callback that delivers
/// the response. The handler must call the callback exactly once, synchronously, before it returns.
using Handler = std::function<void(const RequestView &, SessionStore &, const Inertia &,
                                   std::function<void(Response)>)>;

/// A route of the conformance application: the method and path a request is matched on, and the
/// handler that answers it.
struct Route {
    /// Creates a route.
    ///
    /// @param verb HTTP method the route answers, in uppercase.
    /// @param pathOnly Path the route answers, without a query string.
    /// @param fn Handler of the route.
    Route(std::string verb, std::string pathOnly, Handler fn)
        : method(std::move(verb)), path(std::move(pathOnly)), handler(std::move(fn)) {}

    /// HTTP method the route answers, in uppercase.
    std::string method;
    /// Path the route answers, without the query string.
    std::string path;
    /// Handler that answers requests to this route.
    Handler handler;
};

/// Configuration of the conformance application.
///
/// Sets the protocol version to "v1" and leaves server-side rendering disabled.
///
/// @return A new configuration, on each call.
[[nodiscard]] Config conformanceConfig();

/// Builds the conformance application.
///
/// The instance is built from conformanceConfig() and shares the prop "app" with the value
/// "cpp-inertia".
///
/// @return A new Inertia instance, ready to be used by the routes.
[[nodiscard]] Inertia makeConformanceApp();

/// Lists the routes that the scenarios call.
///
/// The adapter registers each route with its framework, so that a request with the route's method
/// and path invokes its handler (see routes.cpp for the scenarios behind each route).
///
/// @return The routes of the conformance application.
[[nodiscard]] std::vector<Route> conformanceRoutes();

} // namespace inertia::conformance
