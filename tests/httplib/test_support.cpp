#include "test_support.hpp"

#include <inertia/headers.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace inertia::httplib_test {

TestServer::TestServer() = default;

TestServer::~TestServer() {
    server_.stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void TestServer::start() {
    const int bound = server_.bind_to_any_port("127.0.0.1");
    if (bound <= 0) {
        throw std::runtime_error("cannot bind a loopback port");
    }
    port_ = static_cast<std::uint16_t>(bound);
    thread_ = std::thread([this] { server_.listen_after_bind(); });
    server_.wait_until_ready();
    if (!server_.is_running()) {
        throw std::runtime_error("the httplib test server did not start");
    }
}

std::string TestServer::baseUrl() const { return "http://127.0.0.1:" + std::to_string(port_); }

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
        if (inertia::headers::iequals(key, "Set-Cookie") && value.starts_with(prefix)) {
            return value;
        }
    }
    return {};
}

std::string Reply::cookieValue(std::string_view name) const {
    const auto line = setCookie(name);
    if (line.empty()) {
        return {};
    }
    const auto from = name.size() + 1;
    return line.substr(from, line.find(';', from) - from);
}

Client::Client(std::uint16_t port) : port_(port) {}

Reply Client::send(const Request &req) {
    ::httplib::Client http("127.0.0.1", port_);
    http.set_connection_timeout(std::chrono::seconds(5));
    http.set_read_timeout(std::chrono::seconds(10));
    http.set_write_timeout(std::chrono::seconds(5));

    ::httplib::Request request;
    request.method = req.method;
    request.path = req.url;
    request.body = req.body;
    bool hasCookie = false;
    for (const auto &[name, value] : req.headers) {
        hasCookie = hasCookie || inertia::headers::iequals(name, "Cookie");
        request.headers.emplace(name, value);
    }
    if (!hasCookie && !jar_.empty()) {
        std::string cookie;
        for (const auto &[name, value] : jar_) {
            if (!cookie.empty()) {
                cookie += "; ";
            }
            cookie.append(name).append("=").append(value);
        }
        request.headers.emplace("Cookie", cookie);
    }

    const auto result = http.send(request);
    if (!result) {
        throw std::runtime_error("request failed: " + req.method + " " + req.url + ": " +
                                 ::httplib::to_string(result.error()));
    }
    Reply reply;
    reply.status = result->status;
    reply.body = result->body;
    for (const auto &[name, value] : result->headers) {
        reply.headers.emplace_back(name, value);
        if (!inertia::headers::iequals(name, "Set-Cookie")) {
            continue;
        }
        const auto pair = value.substr(0, value.find(';'));
        const auto eq = pair.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const auto cookieName = pair.substr(0, eq);
        const auto cookieValue = pair.substr(eq + 1);
        if (cookieValue.empty() || value.find("Max-Age=0") != std::string::npos) {
            jar_.erase(cookieName);
        } else {
            jar_[cookieName] = cookieValue;
        }
    }
    return reply;
}

} // namespace inertia::httplib_test
