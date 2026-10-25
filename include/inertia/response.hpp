#pragma once

#include <inertia/attributes.hpp>
#include <inertia/core_export.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

/// Header name and value pairs, in order. A name may repeat.
using Headers = std::vector<std::pair<std::string, std::string>>;

/// An HTTP response built by the framework-neutral core. The adapter converts it to its own
/// response type before sending it.
INERTIA_DLL_INTERFACE_BEGIN
struct INERTIA_CORE_EXPORT Response {
    /// HTTP status code.
    int status = 200;
    /// Header name and value pairs, in order. A name may repeat, so use setHeader() to replace one.
    Headers headers;
    /// Response body. Empty by default.
    std::string body;

    /// Looks up a header.
    /// @param name Header name, compared case-insensitively.
    /// @return The value of the first matching header, or nullopt when there is none.
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const;

    /// Sets a header, replacing any existing headers with the same name.
    /// @param name Header name. Its case is kept as given.
    /// @param value Header value.
    void setHeader(std::string name, std::string value);
};
INERTIA_DLL_INTERFACE_END

} // namespace inertia
