#include <inertia/disk_file_reader.hpp>

#include <inertia/ports/file_reader.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
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
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return std::nullopt;
    }
    const auto modified = std::filesystem::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }
    const auto nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(modified.time_since_epoch());
    return FileStamp{.size = size, .modified = static_cast<std::int64_t>(nanoseconds.count())};
}

} // namespace inertia
