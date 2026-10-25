#pragma once

#include <cstddef>
#include <string>

namespace inertia::httplib::detail {

// `bytes` bytes from the operating system's CSPRNG as lowercase hex: getrandom(2) on Linux,
// BCryptGenRandom on Windows, arc4random_buf on macOS and the BSDs. There is no fallback: a
// predictable session id or CSRF token is worse than a failed request, so it throws
// std::runtime_error when the system has no secure randomness.
[[nodiscard]] std::string randomHex(std::size_t bytes);

} // namespace inertia::httplib::detail
