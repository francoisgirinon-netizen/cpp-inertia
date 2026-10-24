#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace inertia::headers {

/// Marks a request as an Inertia request. Its presence, with any value, is enough.
inline constexpr std::string_view kInertia = "X-Inertia";
/// Asset version the client holds. handleVersion() compares it with the server's version.
inline constexpr std::string_view kVersion = "X-Inertia-Version";
/// Component of a partial reload. The reload is partial only when it matches the component.
inline constexpr std::string_view kPartialComponent = "X-Inertia-Partial-Component";
/// Comma-separated prop paths the client wants on a partial reload (`only`).
inline constexpr std::string_view kPartialData = "X-Inertia-Partial-Data";
/// Comma-separated prop paths the client does not want on a partial reload (`except`).
inline constexpr std::string_view kPartialExcept = "X-Inertia-Partial-Except";
/// Comma-separated prop paths whose merge state is reset. Their merge metadata is suppressed.
inline constexpr std::string_view kReset = "X-Inertia-Reset";
/// Name of the error bag that the request's validation errors are scoped to.
inline constexpr std::string_view kErrorBag = "X-Inertia-Error-Bag";
/// Scroll-merge direction: `prepend` to add new items before the list, anything else to append.
inline constexpr std::string_view kMergeIntent = "X-Inertia-Infinite-Scroll-Merge-Intent";
/// Comma-separated keys of the once props the client already holds. Their values are omitted.
inline constexpr std::string_view kExceptOnce = "X-Inertia-Except-Once-Props";
/// Response header with the URL of an external navigation. Sent with a 409 status.
inline constexpr std::string_view kLocation = "X-Inertia-Location";
/// Response header with the URL of a redirect that the client must perform itself, such as one
/// with a fragment. Sent with a 409 status.
inline constexpr std::string_view kRedirect = "X-Inertia-Redirect";
/// Request header that marks a prefetch. Prefetches are exempt from the fragment-redirect rule.
inline constexpr std::string_view kPurpose = "Purpose";
/// Response header that lists the request headers the response depends on.
inline constexpr std::string_view kVary = "Vary";

/// Splits a comma-separated header value into its items.
/// @param raw Header value, or nullopt when the header is absent.
/// @return The items, trimmed of surrounding whitespace. Empty items are dropped. Empty when the
/// header is absent.
[[nodiscard]] std::vector<std::string> parseList(const std::optional<std::string> &raw);

/// Compares two strings ignoring ASCII case.
/// @param a First string.
/// @param b Second string.
/// @return `true` when both strings match, ignoring ASCII case.
[[nodiscard]] bool iequals(std::string_view a, std::string_view b);

} // namespace inertia::headers
