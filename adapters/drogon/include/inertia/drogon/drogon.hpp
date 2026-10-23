#pragma once

#include <inertia/inertia.hpp>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/Session.h>

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

// Stores each value as its dumped JSON text inside the Drogon session. Drogon's Session has its
// own lock, so a store is safe to use while other requests of the same session run.
class DrogonSession final : public SessionStore {
  public:
    explicit DrogonSession(::drogon::SessionPtr session);

    [[nodiscard]] std::optional<Json> get(std::string_view key) const override;
    void put(std::string key, Json value) override;
    std::optional<Json> pull(std::string_view key) override;

  private:
    ::drogon::SessionPtr session_;
};

} // namespace inertia::drogon
