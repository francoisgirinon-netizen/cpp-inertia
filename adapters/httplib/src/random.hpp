#pragma once

#include <cstddef>
#include <string>

namespace inertia::httplib::detail {

// `bytes` bytes from the kernel CSPRNG (getrandom(2)), as lowercase hex. There is no fallback: a
// predictable session id or CSRF token is worse than a failed request, so it throws
// std::runtime_error when the system has no secure randomness.
[[nodiscard]] std::string randomHex(std::size_t bytes);

} // namespace inertia::httplib::detail
