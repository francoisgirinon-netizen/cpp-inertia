#include "random.hpp"

#include <sys/random.h>

#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace inertia::httplib::detail {

std::string randomHex(std::size_t bytes) {
    std::vector<std::byte> buffer(bytes);
    std::size_t done = 0;
    while (done < buffer.size()) {
        const auto got = ::getrandom(buffer.data() + done, buffer.size() - done, 0);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("getrandom failed: no secure random source");
        }
        done += static_cast<std::size_t>(got);
    }
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes * 2);
    for (const auto byte : buffer) {
        const auto value = std::to_integer<unsigned>(byte);
        out += digits[value >> 4U];
        out += digits[value & 0xFU];
    }
    return out;
}

} // namespace inertia::httplib::detail
