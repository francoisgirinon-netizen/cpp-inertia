// The example on the Drogon adapter: a thin layer that registers the shared routes (routes.hpp)
// with Drogon.

#include "app.hpp"
#include "context.hpp"
#include "routes.hpp"

#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/response.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>
#include <drogon/drogon_callbacks.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using Callback = std::function<void(const ::drogon::HttpResponsePtr &)>;

// The request as the shared handlers see it.
class DrogonContext final : public example::Context {
  public:
    DrogonContext(::drogon::HttpRequestPtr req, std::vector<std::string> paramNames)
        : req_{std::move(req)}, paramNames_{std::move(paramNames)}, view_{req_} {}

    [[nodiscard]] std::string query(const std::string &name) const override {
        return req_->getParameter(name);
    }

    [[nodiscard]] std::string pathParam(const std::string &name) const override {
        const auto found = std::ranges::find(paramNames_, name);
        const auto &values = req_->getRoutingParameters();
        const auto index = static_cast<std::size_t>(found - paramNames_.begin());
        return found != paramNames_.end() && index < values.size() ? values[index] : std::string{};
    }

    [[nodiscard]] std::string body() const override { return std::string{req_->body()}; }

    [[nodiscard]] const inertia::RequestView &request() const override { return view_; }

    [[nodiscard]] inertia::SessionStore &session() override {
        if (!session_) {
            session_.emplace(req_->session());
        }
        return *session_;
    }

  private:
    ::drogon::HttpRequestPtr req_;
    std::vector<std::string> paramNames_;
    inertia::drogon::DrogonRequest view_;
    std::optional<inertia::drogon::DrogonSession> session_;
};

// Drogon patterns spell path parameters `{name}`; the shared routes spell them `:name`.
struct Pattern {
    std::string drogon;
    std::vector<std::string> names;
};

Pattern translate(const std::string &pattern) {
    Pattern out;
    std::size_t pos = 0;
    while (pos < pattern.size()) {
        if (pattern[pos] == ':') {
            const auto end = std::min(pattern.find('/', pos), pattern.size());
            out.names.push_back(pattern.substr(pos + 1, end - pos - 1));
            out.drogon += "{" + out.names.back() + "}";
            pos = end;
        } else {
            out.drogon += pattern[pos++];
        }
    }
    return out;
}

::drogon::HttpMethod toDrogon(example::Method method) {
    switch (method) {
    case example::Method::Get:
        return ::drogon::Get;
    case example::Method::Post:
        return ::drogon::Post;
    case example::Method::Put:
        return ::drogon::Put;
    }
    return ::drogon::Get;
}

class DrogonRouter final : public example::Router {
  public:
    void route(example::Method method, std::string pattern, example::Handler handler) override {
        auto translated = translate(pattern);
        ::drogon::app().registerHandler(
            translated.drogon,
            [run = std::move(handler), names = std::move(translated.names)](
                const ::drogon::HttpRequestPtr &req, Callback &&done) {
                DrogonContext ctx{req, names};
                auto outcome = run(ctx);
                if (auto *page = std::get_if<example::Page>(&outcome)) {
                    inertia::drogon::render(req, std::move(page->component), std::move(page->props),
                                            std::move(done), page->options);
                } else if (auto *location = std::get_if<example::Location>(&outcome)) {
                    std::move(done)(
                        inertia::drogon::toDrogon(inertia::drogon::location(req, location->url)));
                } else {
                    std::move(done)(
                        inertia::drogon::toDrogon(std::get<inertia::Response>(outcome)));
                }
            },
            {toDrogon(method)});
    }
};

} // namespace

int main() {
    auto inertiaApp = example::makeInertia(std::make_shared<inertia::drogon::DrogonHttpClient>());

    auto &app = ::drogon::app();
    app.enableSession(std::chrono::hours(1));
    app.setThreadNum(example::kThreads);
    app.setDocumentRoot(example::publicDir()); // serves /build/assets/*

    DrogonRouter router;
    example::registerRoutes(router);

    // Must come last: Inertia is immutable once installed.
    inertia::drogon::install(inertiaApp);

    app.addListener("0.0.0.0", example::kPort);
    app.run();
    return 0;
}
