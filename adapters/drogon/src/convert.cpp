#include <inertia/drogon/drogon.hpp>

#include <inertia/headers.hpp>

#include <drogon/Cookie.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace inertia::drogon {

namespace {

std::string lower(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return out;
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

// "name=value; Path=/; SameSite=Lax" -> a cookie; nullopt when there is no name.
std::optional<::drogon::Cookie> parseSetCookie(std::string_view line) {
    const auto semi = line.find(';');
    const auto pair = trim(line.substr(0, semi));
    const auto eq = pair.find('=');
    const auto name = trim(pair.substr(0, eq));
    if (eq == std::string_view::npos || name.empty()) {
        return std::nullopt;
    }
    ::drogon::Cookie cookie{std::string{name}, std::string{trim(pair.substr(eq + 1))}};

    line = semi == std::string_view::npos ? std::string_view{} : line.substr(semi + 1);
    while (!line.empty()) {
        const auto next = line.find(';');
        const auto attr = trim(line.substr(0, next));
        line = next == std::string_view::npos ? std::string_view{} : line.substr(next + 1);
        const auto sep = attr.find('=');
        const auto key = trim(attr.substr(0, sep));
        const auto value =
            sep == std::string_view::npos ? std::string_view{} : trim(attr.substr(sep + 1));
        if (headers::iequals(key, "Path")) {
            cookie.setPath(std::string{value});
        } else if (headers::iequals(key, "Domain")) {
            cookie.setDomain(std::string{value});
        } else if (headers::iequals(key, "HttpOnly")) {
            cookie.setHttpOnly(true);
        } else if (headers::iequals(key, "Secure")) {
            cookie.setSecure(true);
        } else if (headers::iequals(key, "SameSite")) {
            cookie.setSameSite(::drogon::Cookie::convertString2SameSite(value));
        } else if (headers::iequals(key, "Max-Age")) {
            int seconds = 0;
            if (std::from_chars(value.data(), value.data() + value.size(), seconds).ec ==
                std::errc{}) {
                cookie.setMaxAge(seconds);
            }
        }
    }
    return cookie;
}

} // namespace

DrogonRequest::DrogonRequest(::drogon::HttpRequestPtr req) : req_(std::move(req)) {}

std::string_view DrogonRequest::method() const { return req_->methodString(); }

std::string DrogonRequest::url() const {
    std::string out = req_->getOriginalPath();
    if (const auto &query = req_->query(); !query.empty()) {
        out += '?';
        out += query;
    }
    return out;
}

std::string DrogonRequest::fullUrl() const {
    const auto host = header("Host").value_or("localhost");
    return std::string{req_->isOnSecureConnection() ? "https://" : "http://"} + host + url();
}

std::optional<std::string> DrogonRequest::header(std::string_view name) const {
    const auto &all = req_->getHeaders();
    const auto it = all.find(lower(name));
    if (it == all.end()) {
        return std::nullopt;
    }
    return it->second;
}

::drogon::HttpResponsePtr toDrogon(const Response &res) {
    auto out = ::drogon::HttpResponse::newHttpResponse();
    // The enum is open-ended: codes Drogon has no name for (e.g. 419) are still sent as-is.
    out->setStatusCode(static_cast<::drogon::HttpStatusCode>(res.status));
    out->setBody(res.body);
    for (const auto &[name, value] : res.headers) {
        if (headers::iequals(name, "Content-Type")) {
            out->setContentTypeString(value);
        } else if (headers::iequals(name, "Set-Cookie")) {
            if (auto cookie = parseSetCookie(value)) {
                out->addCookie(std::move(*cookie));
            }
        } else {
            out->addHeader(name, value);
        }
    }
    return out;
}

Response fromDrogon(const ::drogon::HttpResponsePtr &res) {
    Response out;
    out.status = static_cast<int>(res->statusCode());
    out.body = std::string{res->body()};
    if (res->contentType() != ::drogon::CT_NONE) {
        out.headers.emplace_back("Content-Type", res->contentTypeString());
    }
    for (const auto &[name, value] : res->headers()) {
        out.headers.emplace_back(name, value);
    }
    for (const auto &entry : res->cookies()) {
        out.headers.emplace_back("Set-Cookie", entry.second.cookieString());
    }
    return out;
}

} // namespace inertia::drogon
