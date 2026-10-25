#include <inertia/disk_file_reader.hpp>

#include <inertia/ports/file_reader.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ratio>
#include <sstream>
#include <string>
#include <system_error>

namespace inertia {

std::optional<std::string> DiskFileReader::read(const std::filesystem::path &path) const {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return std::nullopt;
    }
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream content;
    content << in.rdbuf();
    return content.str();
}

std::optional<FileStamp> DiskFileReader::stamp(const std::filesystem::path &path) const {
    std::error_code error;
    // file_size() fails on a directory with the POSIX libraries but not with the MSVC one.
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return std::nullopt;
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return std::nullopt;
    }
    const auto modified = std::filesystem::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }
    // Counted in an exactly 64-bit integer, so no cast: the representation of std::chrono::
    // nanoseconds is some signed integer of at least 64 bits, a different type on each library.
    const auto nanoseconds =
        std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::nano>>(
            modified.time_since_epoch());
    return FileStamp{.size = size, .modified = nanoseconds.count()};
}

} // namespace inertia
