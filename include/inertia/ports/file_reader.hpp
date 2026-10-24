#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace inertia {

/// Reads whole files for the Vite integration (the manifest and the hot file).
///
/// Inertia never touches the file system itself: it reads through this port, so the adapter decides
/// how files are accessed. Implementations must be callable from several threads at once.
class FileReader {
  protected:
    FileReader() = default;
    /// Copy-constructs a reader in a derived class.
    FileReader(const FileReader &) = default;
    /// Copy-assigns a reader in a derived class.
    ///
    /// @return `*this`.
    FileReader &operator=(const FileReader &) = default;
    /// Move-constructs a reader in a derived class.
    FileReader(FileReader &&) = default;
    /// Move-assigns a reader in a derived class.
    ///
    /// @return `*this`.
    FileReader &operator=(FileReader &&) = default;

  public:
    /// Virtual so that readers can be destroyed through a FileReader pointer.
    virtual ~FileReader() = default;

    /// Reads the whole content of a file.
    ///
    /// Implementations must report a missing or unreadable file as std::nullopt, not as an
    /// exception.
    ///
    /// @param path File to read.
    /// @return The content of the file, or std::nullopt when it cannot be read.
    [[nodiscard]] virtual std::optional<std::string>
    read(const std::filesystem::path &path) const = 0;
};

} // namespace inertia
