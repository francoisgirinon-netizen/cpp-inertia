#pragma once

#include <string_view>

namespace inertia::detail::session_keys {

// Session entries shared between the helpers that write them (session.hpp) and the page render
// that consumes them.
inline constexpr std::string_view kErrors = "inertia.errors";
inline constexpr std::string_view kFlash = "inertia.flash_data";
inline constexpr std::string_view kClearHistory = "inertia.clear_history";
inline constexpr std::string_view kPreserveFragment = "inertia.preserve_fragment";

} // namespace inertia::detail::session_keys
