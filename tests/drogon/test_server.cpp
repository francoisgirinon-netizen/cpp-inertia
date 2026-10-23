#include "test_server.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <inertia/headers.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <memory>
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

sockaddr_in loopback(std::uint16_t value) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(value);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return addr;
}

// Asks the kernel for a free port. ctest starts one process per test case (and may run them in
// parallel), so a fixed port would collide; a probed port cannot.
std::uint16_t probeFreePort() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("socket() failed");
    }
    auto addr = loopback(0);
    socklen_t len = sizeof(addr);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): POSIX sockaddr API.
    auto *generic = reinterpret_cast<sockaddr *>(&addr);
    if (::bind(fd, generic, sizeof(addr)) != 0 || ::getsockname(fd, generic, &len) != 0) {
        ::close(fd);
        throw std::runtime_error("cannot probe a free port");
    }
    ::close(fd);
    return ntohs(addr.sin_port);
}

bool accepts(std::uint16_t value) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    const auto addr = loopback(value);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): POSIX sockaddr API.
    const bool ok = ::connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == 0;
    ::close(fd);
    return ok;
}

class ServerListener : public Catch::EventListenerBase {
  public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo &) override {
        portStorage() = probeFreePort();
        configureApp();
        ::drogon::app()
            .setLogLevel(trantor::Logger::kWarn)
            .setThreadNum(2)
            .disableSigtermHandling()
            .addListener("127.0.0.1", portStorage());
        thread_ = std::thread([] { ::drogon::app().run(); });

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (!accepts(portStorage())) {
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
