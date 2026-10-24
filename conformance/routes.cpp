#include <inertia/conformance/routes.hpp>

#include <inertia/json.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>
#include <inertia/session.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inertia::conformance {

namespace {

constexpr Json::number_integer_t kBigInt = 9007199254740993LL;

// A route that renders `component` with the props built by `makeProps` on each request.
Handler renderPage(std::string name, std::function<Props()> propsFn, RenderOptions opts = {}) {
    return [component = std::move(name), makeProps = std::move(propsFn),
            opts](const RequestView &req, SessionStore &session, const Inertia &app,
                  std::function<void(Response)> done) {
        app.render(req, session, component, makeProps(), std::move(done), opts);
    };
}

Handler
respond(std::function<Response(const RequestView &, SessionStore &, const Inertia &)> action) {
    return [fn = std::move(action)](const RequestView &req, SessionStore &session,
                                    const Inertia &app, const std::function<void(Response)> &done) {
        done(fn(req, session, app));
    };
}

Prop scrolled() {
    return scroll([] { return Json{{"data", Json::array({1, 2})}}; },
                  [] {
                      return ScrollMetadata{.pageName = "page",
                                            .previousPage = nullptr,
                                            .nextPage = 2,
                                            .currentPage = 1};
                  });
}

Props pageProps() { return {{"title", "Home"}, {"count", 1}}; }

Props partialProps() {
    return {{"auth", object({{"user", Json::object({{"id", 1}})}, {"token", "t"}})},
            {"other", 2},
            {"keep", always(1)},
            {"lazy", optional([] { return 5; })},
            {"res", [] { return Json::object({{"x", 1}, {"y", 2}}); }},
            {"dot.a", 1},
            {"dot.b", 2}};
}

Props deferredProps() {
    const auto value = [](int v) { return [v]() -> Prop { return v; }; };
    return {{"x", 1},
            {"f", defer(value(7))},
            {"g", defer(value(8), "side")},
            {"h", defer(value(9), "side")},
            {"auth", object({{"user", 1}, {"notifications", defer(value(2))}})}};
}

Props rescueProps() {
    return {{"f", defer([]() -> Prop { throw std::runtime_error("boom"); }).rescue()}, {"x", 1}};
}

Props mergeProps() {
    return {{"m", merge(Json::array({1}))},   {"p", merge(Json::array({2})).prepend()},
            {"d", deepMerge(Json::object())}, {"n", merge(1).append("data", "id")},
            {"k", merge(1).matchOn("id")},    {"dm", defer([] { return 1; }).merge()}};
}

Props onceProps() {
    return {{"f", once([] { return 1; }).until(std::chrono::seconds(60))},
            {"g", once([] { return 2; }).as("k")},
            {"h", once([] { return 3; }).fresh()},
            {"d", defer([] { return 4; }).once()}};
}

Props formProps() { return {}; }

Response redirectTo(std::string url) { return redirect(std::move(url)); }

} // namespace

Config conformanceConfig() {
    Config config;
    config.version = [] { return std::string("v1"); };
    return config;
}

Inertia makeConformanceApp() {
    Inertia app{conformanceConfig()};
    app.share("app", "cpp-inertia");
    return app;
}

std::vector<Route> conformanceRoutes() {
    RenderOptions encrypted;
    encrypted.encryptHistory = true;
    RenderOptions bigints;
    bigints.preserveBigIntegers = true;

    std::vector<Route> routes{
        {"GET", "/page", renderPage("Page", pageProps)},
        {"GET", "/slash/", renderPage("Slash", pageProps)},
        {"GET", "/escape", renderPage("Escape", [] { return Props{{"s", "</script><!--x-->"}}; })},
        {"GET", "/encrypted", renderPage("Page", pageProps, encrypted)},
        {"GET", "/bigint",
         renderPage(
             "Page", [] { return Props{{"big", kBigInt}}; }, bigints)},
        {"GET", "/partial", renderPage("Partial", partialProps)},
        {"GET", "/deferred", renderPage("Deferred", deferredProps)},
        {"GET", "/rescue", renderPage("Rescue", rescueProps)},
        {"GET", "/merge", renderPage("Merge", mergeProps)},
        {"GET", "/once", renderPage("Once", onceProps)},
        {"GET", "/scroll", renderPage("Scroll", [] { return Props{{"f", scrolled()}}; })},
        {"GET", "/scroll-deferred",
         renderPage("ScrollDeferred", [] { return Props{{"f", scrolled().deferred()}}; })},
        {"GET", "/form", renderPage("Form", formProps)},
        {"POST", "/errors",
         respond([](const RequestView &, SessionStore &session, const Inertia &) {
             withErrors(session, {{"name", {"req", "min"}}});
             return redirectTo("/form");
         })},
        {"POST", "/flash", respond([](const RequestView &, SessionStore &session, const Inertia &) {
             flash(session, "notice", "saved");
             flash(session, "n", 2);
             return redirectTo("/form");
         })},
        {"POST", "/clear-history",
         respond([](const RequestView &, SessionStore &session, const Inertia &) {
             clearHistory(session);
             return redirectTo("/page");
         })},
        {"POST", "/fragment",
         respond([](const RequestView &, SessionStore &session, const Inertia &) {
             preserveFragment(session);
             return redirectTo("/page#section");
         })},
        {"GET", "/external",
         respond([](const RequestView &req, SessionStore &, const Inertia &app) {
             return app.location(req, "https://example.com/away");
         })},
        {"GET", "/empty",
         respond([](const RequestView &, SessionStore &, const Inertia &) { return Response{}; })},
        {"GET", "/zero", respond([](const RequestView &, SessionStore &, const Inertia &) {
             Response res;
             res.body = "0";
             return res;
         })},
        {"GET", "/vary", respond([](const RequestView &, SessionStore &, const Inertia &) {
             Response res;
             res.body = "ok";
             res.setHeader("Vary", "Accept-Encoding");
             return res;
         })},
    };
    // Mutating verbs answer with a redirect, to observe the 302 -> 303 rewrite.
    for (const char *verb : {"PUT", "PATCH", "DELETE", "POST"}) {
        routes.emplace_back(verb, "/update-redirect",
                            respond([](const RequestView &, SessionStore &, const Inertia &) {
                                return redirectTo("/page");
                            }));
    }
    return routes;
}

} // namespace inertia::conformance
