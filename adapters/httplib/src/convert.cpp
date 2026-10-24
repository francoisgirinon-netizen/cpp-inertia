#include <inertia/httplib/httplib.hpp>

#include <inertia/headers.hpp>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace inertia::httplib {

namespace {

std::string upper(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](char c) {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });
    return out;
}

// Something that can stand for the authority of a URL: no whitespace, slashes, '@' or controls.
bool isPlainAuthority(std::string_view host) {
    return !host.empty() && std::ranges::all_of(host, [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '.' || c == '-' ||
               c == '_' || c == ':' || c == '[' || c == ']';
    });
}

} // namespace

HttplibRequest::HttplibRequest(const ::httplib::Request &req, bool trustForwardedHeaders)
    : req_(&req), method_(upper(req.method)), trustForwarded_(trustForwardedHeaders) {}

std::string_view HttplibRequest::method() const { return method_; }

std::string HttplibRequest::url() const {
    if (!req_->target.empty()) {
        return req_->target;
    }
    std::string out = req_->path.empty() ? "/" : req_->path;
    char separator = '?';
    for (const auto &[key, value] : req_->params) {
        out += separator;
        out += ::httplib::encode_query_component(key);
        out += '=';
        out += ::httplib::encode_query_component(value);
        separator = '&';
    }
    return out;
}

std::optional<std::string> HttplibRequest::header(std::string_view name) const {
    const auto [first, last] = req_->headers.equal_range(std::string{name});
    if (first == last) {
        return std::nullopt;
    }
    std::string joined = first->second;
    for (auto it = std::next(first); it != last; ++it) {
        joined += ", ";
        joined += it->second;
    }
    return joined;
}

bool HttplibRequest::isSecure() const {
    if (!trustForwarded_) {
        return false;
    }
    const auto proto = headers::parseList(header("X-Forwarded-Proto"));
    return !proto.empty() && headers::iequals(proto.front(), "https");
}

std::string HttplibRequest::fullUrl() const {
    std::string host;
    if (trustForwarded_) {
        if (const auto forwarded = headers::parseList(header("X-Forwarded-Host"));
            !forwarded.empty() && isPlainAuthority(forwarded.front())) {
            host = forwarded.front();
        }
    }
    if (host.empty()) {
        host = header("Host").value_or("localhost");
    }
    return std::string{isSecure() ? "https://" : "http://"} + host + url();
}

void toHttplib(const Response &from, ::httplib::Response &to) {
    to.status = from.status;
    to.body = from.body;
    to.headers.clear();
    for (const auto &[name, value] : from.headers) {
        if (!headers::iequals(name, "Content-Length")) {
            to.headers.emplace(name, value);
        }
    }
}

Response fromHttplib(const ::httplib::Response &res) {
    Response out;
    out.status = res.status;
    out.body = res.body;
    for (const auto &[name, value] : res.headers) {
        out.headers.emplace_back(name, value);
    }
    return out;
}

} // namespace inertia::httplib
