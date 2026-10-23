#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace inertia::headers {

inline constexpr std::string_view kInertia = "X-Inertia", kVersion = "X-Inertia-Version",
                                  kPartialComponent = "X-Inertia-Partial-Component",
                                  kPartialData = "X-Inertia-Partial-Data",
                                  kPartialExcept = "X-Inertia-Partial-Except",
                                  kReset = "X-Inertia-Reset", kErrorBag = "X-Inertia-Error-Bag",
                                  kMergeIntent = "X-Inertia-Infinite-Scroll-Merge-Intent",
                                  kExceptOnce = "X-Inertia-Except-Once-Props",
                                  kLocation = "X-Inertia-Location",
                                  kRedirect = "X-Inertia-Redirect", kPurpose = "Purpose",
                                  kVary = "Vary";

// Splits a comma-separated header value, trimming whitespace and dropping empty items.
[[nodiscard]] std::vector<std::string> parseList(const std::optional<std::string> &raw);

// ASCII case-insensitive comparison.
[[nodiscard]] bool iequals(std::string_view a, std::string_view b);

} // namespace inertia::headers
