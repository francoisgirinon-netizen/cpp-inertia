#pragma once

#include <inertia/inertia.hpp>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <optional>
#include <string>
#include <string_view>

// Inside this namespace the global Drogon namespace must always be spelled ::drogon.
namespace inertia::drogon {

// Read-only view of a Drogon request. Not thread safe, like the request it wraps.
class DrogonRequest final : public RequestView {
  public:
    explicit DrogonRequest(::drogon::HttpRequestPtr req);

    [[nodiscard]] std::string_view method() const override;
    [[nodiscard]] std::string url() const override;
    [[nodiscard]] std::string fullUrl() const override;
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override;

  private:
    ::drogon::HttpRequestPtr req_;
};

[[nodiscard]] ::drogon::HttpResponsePtr toDrogon(const Response &res);
[[nodiscard]] Response fromDrogon(const ::drogon::HttpResponsePtr &res);

} // namespace inertia::drogon
