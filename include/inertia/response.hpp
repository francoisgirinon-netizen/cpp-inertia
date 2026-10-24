#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

struct Response {
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    // Case-insensitive lookup of the first matching header.
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const;

    // Replaces every header with the same name (case-insensitive), then appends this one.
    void setHeader(std::string name, std::string value);
};

} // namespace inertia
