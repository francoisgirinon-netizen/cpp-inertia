#pragma once

// The framework-agnostic side of the example: handlers are written against this interface and the
// inertia core only. Each adapter (Drogon, cpp-httplib) implements Context and Router in a thin
// registration layer (drogon_main.cpp, httplib_main.cpp).

#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <variant>

namespace example {

// What a handler sees of the current request.
class Context {
  public:
    Context() = default;
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&) = delete;
    Context &operator=(Context &&) = delete;
    virtual ~Context() = default;

    // A query-string parameter, or "" when absent.
    [[nodiscard]] virtual std::string query(const std::string &name) const = 0;
    // A path parameter of the route pattern (`/users/:id/promote` has `id`).
    [[nodiscard]] virtual std::string pathParam(const std::string &name) const = 0;
    [[nodiscard]] virtual std::string body() const = 0;
    [[nodiscard]] virtual const inertia::RequestView &request() const = 0;
    // The session of the request (flash messages, validation errors, ...).
    [[nodiscard]] virtual inertia::SessionStore &session() = 0;
};

// Render an Inertia page.
struct Page {
    std::string component;
    inertia::Props props;
    inertia::RenderOptions options{};
};

// An external redirect: 409 + X-Inertia-Location for Inertia requests, 302 otherwise.
struct Location {
    std::string url;
};

// What a handler decides: a ready response, a page to render or an external location.
using Outcome = std::variant<inertia::Response, Page, Location>;
using Handler = std::function<Outcome(Context &)>;

enum class Method : std::uint8_t { Get, Post, Put };

// Where the routes get registered. Patterns use `:name` for path parameters.
class Router {
  public:
    Router() = default;
    Router(const Router &) = delete;
    Router &operator=(const Router &) = delete;
    Router(Router &&) = delete;
    Router &operator=(Router &&) = delete;
    virtual ~Router() = default;

    virtual void route(Method method, std::string pattern, Handler handler) = 0;

    void get(std::string pattern, Handler handler) {
        route(Method::Get, std::move(pattern), std::move(handler));
    }
    void post(std::string pattern, Handler handler) {
        route(Method::Post, std::move(pattern), std::move(handler));
    }
    void put(std::string pattern, Handler handler) {
        route(Method::Put, std::move(pattern), std::move(handler));
    }
};

struct PageRoute {
    std::string path;
    std::string component;
};

// Registers `GET route.path` rendering `route.component` with the props returned by
// makeProps(context).
template <class MakeProps>
void registerPage(Router &router, const PageRoute &route, MakeProps makeProps) {
    router.get(route.path,
               [name = route.component, props = std::move(makeProps)](Context &ctx) -> Outcome {
                   return Page{.component = name, .props = props(ctx)};
               });
}

// The request's JSON body (what the Inertia client sends for forms), or {} when it is not an
// object.
inertia::Json jsonBody(const Context &ctx);

// The integer query parameter `name`, or `fallback` when absent or not a number.
int intQuery(const Context &ctx, const char *name, int fallback);

} // namespace example
