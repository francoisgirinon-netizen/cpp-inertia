#pragma once

#include <inertia/inertia.hpp>

#include <httplib.h>

#include <optional>
#include <string>
#include <string_view>

// Inside this namespace the global cpp-httplib namespace must always be spelled ::httplib.
namespace inertia::httplib {

// Read-only view of a cpp-httplib request. It borrows the request: keep it alive for as long as the
// view is used. Not thread safe, like the request it wraps.
class HttplibRequest final : public RequestView {
  public:
    // `trustForwardedHeaders`: believe X-Forwarded-Proto and X-Forwarded-Host (first value of the
    // list) when building fullUrl() and deciding isSecure(). Enable it only behind a reverse proxy
    // that sets or strips those headers; a directly exposed server must leave it off, since any
    // client can forge them.
    explicit HttplibRequest(const ::httplib::Request &req, bool trustForwardedHeaders = false);

    [[nodiscard]] std::string_view method() const override;
    // The request target exactly as received (path and raw query). A request that was not parsed
    // off the wire (no raw target) falls back to its decoded path and re-encoded parameters.
    [[nodiscard]] std::string url() const override;
    [[nodiscard]] std::string fullUrl() const override;
    // Several header lines of one name are joined with ", ".
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override;

    // True when a trusted X-Forwarded-Proto says https. The server itself speaks plain HTTP.
    [[nodiscard]] bool isSecure() const;

  private:
    const ::httplib::Request *req_;
    std::string method_;
    bool trustForwarded_;
};

// Replaces the status, headers and body of `to`. Content-Length is dropped (cpp-httplib frames the
// body itself); several headers of one name (Set-Cookie) all survive.
void toHttplib(const Response &from, ::httplib::Response &to);
[[nodiscard]] Response fromHttplib(const ::httplib::Response &res);

} // namespace inertia::httplib
