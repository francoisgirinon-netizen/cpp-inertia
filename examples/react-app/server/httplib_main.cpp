// The example on the cpp-httplib adapter: a thin layer that registers the shared routes
// (routes.hpp) with cpp-httplib. The pages are the same as in drogon_main.cpp.

#include "app.hpp"
#include "context.hpp"
#include "routes.hpp"

#include <inertia/httplib/httplib.hpp>
#include <inertia/inertia.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/response.hpp>

#include <httplib.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace {

// The request as the shared handlers see it.
class HttplibContext final : public example::Context {
  public:
    HttplibContext(const ::httplib::Request &req, ::httplib::Response &res)
        : req_{req}, res_{res}, view_{req} {}

    [[nodiscard]] std::string query(const std::string &name) const override {
        return req_.get_param_value(name);
    }

    [[nodiscard]] std::string pathParam(const std::string &name) const override {
        const auto found = req_.path_params.find(name);
        return found == req_.path_params.end() ? std::string{} : found->second;
    }

    [[nodiscard]] std::string body() const override { return req_.body; }

    [[nodiscard]] const inertia::RequestView &request() const override { return view_; }

    [[nodiscard]] inertia::SessionStore &session() override {
        if (!session_) {
            session_.emplace(inertia::httplib::session(res_));
        }
        return *session_;
    }

  private:
    const ::httplib::Request &req_;
    ::httplib::Response &res_;
    inertia::httplib::HttplibRequest view_;
    std::optional<inertia::httplib::HttplibSession> session_;
};

class HttplibRouter final : public example::Router {
  public:
    explicit HttplibRouter(::httplib::Server &server) : server_{server} {}

    void route(example::Method method, std::string pattern, example::Handler handler) override {
        auto serve = [run = std::move(handler)](const ::httplib::Request &req,
                                                ::httplib::Response &res) {
            HttplibContext ctx{req, res};
            auto outcome = run(ctx);
            if (auto *page = std::get_if<example::Page>(&outcome)) {
                inertia::httplib::render(req, res, std::move(page->component),
                                         std::move(page->props), page->options);
            } else if (auto *location = std::get_if<example::Location>(&outcome)) {
                inertia::httplib::location(req, res, location->url);
            } else {
                inertia::httplib::toHttplib(std::get<inertia::Response>(outcome), res);
            }
        };
        switch (method) {
        case example::Method::Get:
            server_.Get(pattern, std::move(serve));
            break;
        case example::Method::Post:
            server_.Post(pattern, std::move(serve));
            break;
        case example::Method::Put:
            server_.Put(pattern, std::move(serve));
            break;
        }
    }

  private:
    ::httplib::Server &server_;
};

} // namespace

int main() {
    auto inertiaApp = example::makeInertia(std::make_shared<inertia::httplib::HttplibHttpClient>());

    ::httplib::Server server;
    // Serves /build/assets/*; the rest of the public directory (the hot file) stays private.
    if (!server.set_mount_point("/build", example::publicDir() + "/build")) {
        std::cerr << "cannot serve " << example::publicDir() << "/build\n";
        return 1;
    }

    HttplibRouter router{server};
    example::registerRoutes(router);
    inertia::httplib::install(server, inertiaApp);

    if (!server.listen("0.0.0.0", example::kPort)) {
        std::cerr << "cannot listen on port " << example::kPort << '\n';
        return 1;
    }
    return 0;
}
