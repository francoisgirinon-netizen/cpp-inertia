#include "app.hpp"

#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <httplib.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace inertia::httplib_test {

namespace {

using Req = ::httplib::Request;
using Res = ::httplib::Response;

void text(Res &res, std::string body) { res.set_content(std::move(body), "text/plain"); }

} // namespace

inertia::httplib::Options App::defaultOptions() {
    inertia::httplib::Options options;
    options.csrf.exceptPaths = {"/echo"};
    return options;
}

App::App(inertia::httplib::Options options) {
    if (!options.sessions) {
        options.sessions = std::make_shared<inertia::httplib::MemorySessionStore>();
    }
    sessions_ = options.sessions;

    Config config;
    config.version = [] { return std::string("v1"); };
    inertia::httplib::install(server_.server(), std::make_shared<Inertia>(std::move(config)),
                              std::move(options));

    auto &raw = server_.server();
    raw.Get("/page", [](const Req &req, Res &res) {
        inertia::httplib::render(req, res, "Page", Props{{"title", "Home"}, {"count", 1}});
    });
    raw.Get("/throws", [](const Req &req, Res &res) {
        inertia::httplib::render(req, res, "Boom", Props{{"ok", 1}, {"boom", failingProp}});
    });
    raw.Get("/external", [](const Req &req, Res &res) {
        inertia::httplib::location(req, res, "https://example.com/away");
    });
    raw.Get("/vary", [](const Req &, Res &res) {
        text(res, "ok");
        res.set_header("Vary", "Accept-Encoding");
    });
    raw.Get("/boom", [](const Req &, Res &) { throw std::runtime_error("handler failed"); });

    // GET /session/put?v=<json> stores the JSON under "k".
    raw.Get("/session/put", [](const Req &req, Res &res) {
        auto session = inertia::httplib::session(res);
        session.put("k", Json::parse(req.get_param_value("v")));
        text(res, "stored");
    });
    raw.Get("/session/get", [](const Req &, Res &res) {
        const auto session = inertia::httplib::session(res);
        const auto value = session.get("k");
        text(res, value ? value->dump() : "missing");
    });
    raw.Get("/session/pull", [](const Req &, Res &res) {
        auto session = inertia::httplib::session(res);
        const auto value = session.pull("k");
        text(res, value ? value->dump() : "missing");
    });

    // POST /echo answers with the request body.
    raw.Post("/echo", [](const Req &req, Res &res) { text(res, req.body); });
    const auto submit = [](const Req &, Res &res) { text(res, "submitted"); };
    raw.Get("/submit", submit);
    raw.Post("/submit", submit);
    raw.Put("/submit", submit);
    raw.Patch("/submit", submit);
    raw.Delete("/submit", submit);
    raw.Get("/empty", [](const Req &, Res &res) { res.status = 200; });
    raw.Get("/ping", [](const Req &, Res &res) { text(res, "pong"); }); // never touches the session
    // Touches the session, so it gets the session and XSRF cookies (like any rendered page).
    raw.Get("/token", [](const Req &, Res &res) {
        (void)inertia::httplib::session(res);
        text(res, "token");
    });
    // Stores a value, then rotates the session id (what a login handler does).
    raw.Get("/regenerate", [](const Req &, Res &res) {
        auto session = inertia::httplib::session(res);
        session.put("k", 7);
        inertia::httplib::regenerateSession(res);
        text(res, "rotated");
    });
    raw.Get("/slow", [](const Req &, Res &res) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        text(res, "late");
    });
    server_.start();
}

} // namespace inertia::httplib_test
