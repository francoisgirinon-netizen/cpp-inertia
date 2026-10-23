#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace inertia {

class RequestView {
  protected:
    RequestView() = default;
    RequestView(const RequestView &) = default;
    RequestView &operator=(const RequestView &) = default;
    RequestView(RequestView &&) = default;
    RequestView &operator=(RequestView &&) = default;

  public:
    virtual ~RequestView() = default;

    [[nodiscard]] virtual std::string_view method() const = 0; // "GET", "POST", ...
    [[nodiscard]] virtual std::string url() const = 0;         // path + query, exactly as received
    [[nodiscard]] virtual std::string fullUrl() const = 0;     // scheme://host + url()
    [[nodiscard]] virtual std::optional<std::string>
    header(std::string_view name) const = 0; // case-insensitive
};

} // namespace inertia
