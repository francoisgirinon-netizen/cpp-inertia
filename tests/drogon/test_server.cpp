#include "test_server.hpp"

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <inertia/headers.hpp>

#include <drogon/Cookie.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <trantor/utils/Logger.h>

#include <loopback.hpp>

#include <chrono>
#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace inertia::drogon_test {

namespace {

std::uint16_t &portStorage() {
    static std::uint16_t value = 0; // written before the server starts, read-only afterwards
    return value;
}

class ServerListener : public Catch::EventListenerBase {
  public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo &) override {
        portStorage() = test_loopback::probeFreePort();
        configureApp();
        ::drogon::app()
            .setLogLevel(trantor::Logger::kWarn)
            .setThreadNum(2)
            .disableSigtermHandling()
            .addListener("127.0.0.1", portStorage());
        thread_ = std::thread([] { ::drogon::app().run(); });

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (!test_loopback::acceptsConnections(portStorage())) {
            if (std::chrono::steady_clock::now() > deadline) {
                throw std::runtime_error("Drogon did not start listening");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void testRunEnded(const Catch::TestRunStats &) override {
        ::drogon::app().quit();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

  private:
    std::thread thread_;
};

} // namespace

std::uint16_t port() { return portStorage(); }

std::string Reply::header(std::string_view name) const {
    for (const auto &[key, value] : headers) {
        if (inertia::headers::iequals(key, name)) {
            return value;
        }
    }
    return {};
}

std::string Reply::setCookie(std::string_view name) const {
    const std::string prefix = std::string{name} + "=";
    for (const auto &[key, value] : headers) {
        if (!inertia::headers::iequals(key, "Set-Cookie")) {
            continue;
        }
        const auto at = value.find(prefix);
        if (at != std::string::npos && (at == 0 || value[at - 1] == ' ' || value[at - 1] == ':')) {
            return value;
        }
    }
    return {};
}

std::string Reply::cookieValue(std::string_view name) const {
    const auto line = setCookie(name);
    const auto start = line.find(std::string{name} + "=");
    if (start == std::string::npos) {
        return {};
    }
    const auto from = start + name.size() + 1;
    return line.substr(from, line.find(';', from) - from);
}

Client::Client() {
    loopThread_.run();
    resetCookies();
}

void Client::resetCookies() {
    client_ =
        ::drogon::HttpClient::newHttpClient("127.0.0.1", port(), false, loopThread_.getLoop());
    client_->enableCookies();
}

Reply Client::send(const Request &req) {
    auto request = ::drogon::HttpRequest::newHttpRequest();
    request->setMethod(req.method == "POST"     ? ::drogon::Post
                       : req.method == "PUT"    ? ::drogon::Put
                       : req.method == "PATCH"  ? ::drogon::Patch
                       : req.method == "DELETE" ? ::drogon::Delete
                       : req.method == "HEAD"   ? ::drogon::Head
                                                : ::drogon::Get);
    request->setPathEncode(false);
    request->setPath(req.url);
    for (const auto &[name, value] : req.headers) {
        request->addHeader(name, value);
    }
    if (!req.body.empty()) {
        request->setBody(req.body);
    }
    const auto [result, response] = client_->sendRequest(request, 10.0);
    if (result != ::drogon::ReqResult::Ok) {
        throw std::runtime_error("request failed: " + req.method + " " + req.url);
    }
    Reply reply;
    reply.status = static_cast<int>(response->statusCode());
    reply.body = std::string{response->body()};
    if (response->contentType() != ::drogon::CT_NONE) {
        reply.headers.emplace_back("Content-Type", response->contentTypeString());
    }
    for (const auto &[name, value] : response->headers()) {
        reply.headers.emplace_back(name, value);
    }
    for (const auto &entry : response->cookies()) {
        reply.headers.emplace_back("Set-Cookie", entry.second.cookieString());
    }
    return reply;
}

} // namespace inertia::drogon_test

CATCH_REGISTER_LISTENER(inertia::drogon_test::ServerListener)
