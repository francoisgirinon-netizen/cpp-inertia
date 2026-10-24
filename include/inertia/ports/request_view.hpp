#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace inertia {

/// Read-only view of the framework's HTTP request, as the core sees it.
///
/// An adapter implements this port around its framework's request object and must not modify that
/// request. The view usually borrows the request, so it must not outlive it. Implementations need
/// not be thread safe: a request is handled by one thread at a time.
class RequestView {
  protected:
    RequestView() = default;
    /// Copy-constructs a view in a derived class.
    RequestView(const RequestView &) = default;
    /// Copy-assigns a view in a derived class.
    ///
    /// @return `*this`.
    RequestView &operator=(const RequestView &) = default;
    /// Move-constructs a view in a derived class.
    RequestView(RequestView &&) = default;
    /// Move-assigns a view in a derived class.
    ///
    /// @return `*this`.
    RequestView &operator=(RequestView &&) = default;

  public:
    /// Virtual so that views can be destroyed through a RequestView pointer.
    virtual ~RequestView() = default;

    /// The HTTP method, in uppercase.
    ///
    /// Implementations must return "GET", "POST", ... exactly: the core compares it verbatim.
    ///
    /// @return The method of the request.
    [[nodiscard]] virtual std::string_view method() const = 0;

    /// The path and query string, exactly as received.
    ///
    /// Implementations must not decode or normalize it: it becomes the `url` of the page object.
    ///
    /// @return The request target, such as "/users?page=2".
    [[nodiscard]] virtual std::string url() const = 0;

    /// The absolute URL of the request: `scheme://host` followed by url().
    ///
    /// Used for absolute URLs, such as the location of an external redirect and the SSR request.
    ///
    /// @return The absolute URL, such as "https://example.com/users?page=2".
    [[nodiscard]] virtual std::string fullUrl() const = 0;

    /// Looks up a request header.
    ///
    /// Implementations must match the name case-insensitively (see inertia::headers::iequals) and
    /// join the values of a repeated header with ", ".
    ///
    /// @param name Header name, in any case.
    /// @return The value, or std::nullopt when the request does not carry the header.
    [[nodiscard]] virtual std::optional<std::string> header(std::string_view name) const = 0;
};

} // namespace inertia
