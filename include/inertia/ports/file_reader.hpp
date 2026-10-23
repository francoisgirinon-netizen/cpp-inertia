#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace inertia {

class FileReader {
  protected:
    FileReader() = default;
    FileReader(const FileReader &) = default;
    FileReader &operator=(const FileReader &) = default;
    FileReader(FileReader &&) = default;
    FileReader &operator=(FileReader &&) = default;

  public:
    virtual ~FileReader() = default;

    [[nodiscard]] virtual std::optional<std::string>
    read(const std::filesystem::path &path) const = 0;
};

} // namespace inertia
