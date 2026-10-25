#include "test_support.hpp"

#include <inertia/headers.hpp>
#include <inertia/httplib/detail/httplib_include.hpp>

#include <loopback.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace inertia::httplib_test {

Json failingProp() { throw std::runtime_error("prop failed"); }

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
    // Hand-written HTTP/1.1, so that the request target reaches the server byte for byte
    // (cpp-httplib's own client normalizes the query).
    std::string wire = req.method + " " + req.url + " HTTP/1.1\r\n";
    wire += "Host: 127.0.0.1:" + std::to_string(port_) + "\r\nConnection: close\r\n";
    bool hasCookie = false;
    bool hasLength = false;
    for (const auto &[name, value] : req.headers) {
        hasCookie = hasCookie || inertia::headers::iequals(name, "Cookie");
        hasLength = hasLength || inertia::headers::iequals(name, "Content-Length");
        wire.append(name).append(": ").append(value).append("\r\n");
    }
    if (!hasCookie && !jar_.empty()) {
        wire += "Cookie: ";
        bool first = true;
        for (const auto &[name, value] : jar_) {
            wire.append(first ? "" : "; ").append(name).append("=").append(value);
            first = false;
        }
        wire += "\r\n";
    }
    if (!hasLength && (!req.body.empty() || req.method == "POST" || req.method == "PUT" ||
                       req.method == "PATCH")) {
        wire += "Content-Length: " + std::to_string(req.body.size()) + "\r\n";
    }
    wire += "\r\n";
    wire += req.body;

    const auto raw = rawExchange(port_, wire);
    const auto headEnd = raw.find("\r\n\r\n");
    const auto lineEnd = raw.find("\r\n");
    if (headEnd == std::string::npos || !raw.starts_with("HTTP/")) {
        throw std::runtime_error("malformed reply to " + req.method + " " + req.url);
    }
    Reply reply;
    const auto space = raw.find(' ');
    reply.status = std::stoi(raw.substr(space + 1, 3));
    reply.body = raw.substr(headEnd + 4);
    std::size_t at = lineEnd + 2;
    while (at < headEnd + 2) {
        const auto next = raw.find("\r\n", at);
        const auto line = raw.substr(at, next - at);
        at = next + 2;
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const auto name = line.substr(0, colon);
        const auto first = line.find_first_not_of(" \t", colon + 1);
        const auto value = first == std::string::npos ? std::string{} : line.substr(first);
        reply.headers.emplace_back(name, value);
        if (inertia::headers::iequals(name, "Set-Cookie")) {
            rememberCookie(value);
        }
    }
    return reply;
}

void Client::rememberCookie(const std::string &setCookie) {
    const auto pair = setCookie.substr(0, setCookie.find(';'));
    const auto eq = pair.find('=');
    if (eq == std::string::npos) {
        return;
    }
    const auto name = pair.substr(0, eq);
    const auto value = pair.substr(eq + 1);
    if (value.empty() || setCookie.find("Max-Age=0") != std::string::npos) {
        jar_.erase(name);
    } else {
        jar_[name] = value;
    }
}

std::string rawExchange(std::uint16_t port, const std::string &request) {
    return test_loopback::rawExchange(port, request);
}

} // namespace inertia::httplib_test
