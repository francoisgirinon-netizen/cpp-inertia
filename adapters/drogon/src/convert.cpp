#include <inertia/drogon/drogon.hpp>

#include <inertia/headers.hpp>

#include <drogon/Cookie.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

// "Wed, 21 Oct 2026 07:28:00 GMT" (the IMF-fixdate of RFC 9110) -> a date; nullopt otherwise.
std::optional<trantor::Date> parseHttpDate(std::string_view text) {
    std::tm parts{};
    const std::string copy{text};
    const char *end = strptime(copy.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &parts);
    if (end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    const auto seconds = timegm(&parts);
    return trantor::Date{static_cast<std::int64_t>(seconds) * 1000000};
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
        } else if (headers::iequals(key, "Expires")) {
            if (const auto date = parseHttpDate(value)) {
                cookie.setExpiresDate(*date);
            }
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

DrogonRequest::DrogonRequest(::drogon::HttpRequestPtr req, bool trustForwardedHeaders)
    : req_(std::move(req)), trustForwarded_(trustForwardedHeaders) {}

namespace {

std::string_view firstValue(std::string_view list) { return trim(list.substr(0, list.find(','))); }

bool plausibleHost(std::string_view host) {
    return !host.empty() && std::ranges::all_of(host, [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '.' || c == '-' ||
               c == ':' || c == '[' || c == ']' || c == '_';
    });
}

} // namespace

bool DrogonRequest::isSecure() const {
    if (trustForwarded_) {
        if (const auto proto = header("X-Forwarded-Proto")) {
            const auto value = lower(firstValue(*proto));
            if (value == "https") {
                return true;
            }
            if (value == "http") {
                return false;
            }
        }
    }
    return req_->isOnSecureConnection();
}

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
    std::string host = header("Host").value_or("localhost");
    if (trustForwarded_) {
        if (const auto forwarded = header("X-Forwarded-Host")) {
            if (const auto first = firstValue(*forwarded); plausibleHost(first)) {
                host = std::string{first};
            }
        }
    }
    return std::string{isSecure() ? "https://" : "http://"} + host + url();
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
    // Drogon keeps one value per header name, so repeated headers are combined into one field
    // with commas (RFC 9110 section 5.3). Cookies are separate objects and stay separate.
    std::vector<std::pair<std::string, std::string>> combined;
    for (const auto &[name, value] : res.headers) {
        if (headers::iequals(name, "Set-Cookie")) {
            if (auto cookie = parseSetCookie(value)) {
                out->addCookie(std::move(*cookie));
            }
            continue;
        }
        const auto same = std::ranges::find_if(
            combined, [&](const auto &entry) { return headers::iequals(entry.first, name); });
        if (same == combined.end()) {
            combined.emplace_back(name, value);
        } else if (headers::iequals(name, "Content-Type")) {
            same->second = value; // not a list header: the last one wins
        } else {
            same->second += ", " + value;
        }
    }
    for (const auto &[name, value] : combined) {
        if (headers::iequals(name, "Content-Type")) {
            out->setContentTypeString(value);
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
