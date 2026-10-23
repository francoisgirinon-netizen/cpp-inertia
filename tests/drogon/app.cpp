#include "test_server.hpp"

#include <inertia/drogon/drogon.hpp>

#include <drogon/HttpAppFramework.h>

#include <functional>
#include <utility>

namespace inertia::drogon_test {

void configureApp() {
    auto &app = ::drogon::app();
    app.enableSession(std::chrono::minutes(10));

    using Callback = std::function<void(const ::drogon::HttpResponsePtr &)>;
    const auto text = [](std::string body) {
        auto res = ::drogon::HttpResponse::newHttpResponse();
        res->setBody(std::move(body));
        return res;
    };

    // PUT /session/put?v=<json> stores the JSON under "k".
    app.registerHandler("/session/put",
                        [text](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            inertia::drogon::DrogonSession session{req->session()};
                            session.put("k", Json::parse(req->getParameter("v")));
                            std::move(done)(text("stored"));
                        },
                        {::drogon::Get});
    app.registerHandler("/session/get",
                        [text](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            const inertia::drogon::DrogonSession session{req->session()};
                            const auto value = session.get("k");
                            std::move(done)(text(value ? value->dump() : "missing"));
                        },
                        {::drogon::Get});
    app.registerHandler("/session/pull",
                        [text](const ::drogon::HttpRequestPtr &req, Callback &&done) {
                            inertia::drogon::DrogonSession session{req->session()};
                            const auto value = session.pull("k");
                            std::move(done)(text(value ? value->dump() : "missing"));
                        },
                        {::drogon::Get});
}

} // namespace inertia::drogon_test
