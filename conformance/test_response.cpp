#include <inertia/conformance/harness.hpp>
#include <inertia/headers.hpp>
#include <inertia/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia::conformance {

std::optional<std::string> TestResponse::header(std::string_view name) const {
    for (const auto &[key, value] : headers) {
        if (headers::iequals(key, name)) {
            return value;
        }
    }
    return std::nullopt;
}

Json TestResponse::json() const { return Json::parse(body); }

} // namespace inertia::conformance
