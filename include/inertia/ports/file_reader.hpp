#pragma once

#include <inertia/core_export.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace inertia {

/// Cheap fingerprint of a file, such as its size and modification time. Two stamps of the same
/// path are equal when the file has not changed in between.
struct FileStamp {
    /// Size of the file in bytes.
    std::uint64_t size = 0;
    /// Modification time, in any unit the reader likes, or any value that changes whenever the
    /// content does.
    std::int64_t modified = 0;

    /// @param a First stamp.
    /// @param b Second stamp.
    /// @return `true` when both stamps are equal.
    friend bool operator==(const FileStamp &a, const FileStamp &b) {
        return a.size == b.size && a.modified == b.modified;
    }
    /// @param a First stamp.
    /// @param b Second stamp.
    /// @return `true` when the stamps differ.
    friend bool operator!=(const FileStamp &a, const FileStamp &b) { return !(a == b); }
};

/// Reads whole files for the Vite integration (the manifest and the hot file).
///
/// Inertia never touches the file system itself: it reads through this port, so the adapter decides
/// how files are accessed. Implementations must be callable from several threads at once.
class INERTIA_CORE_EXPORT FileReader {
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

    /// Fingerprints a file without reading it, so that the Vite integration can keep what it
    /// derived from the manifest (its parsed tags and its hash) until the file changes.
    ///
    /// The default returns std::nullopt, which means "unknown": the file is then read again on
    /// every use and compared with the previous content. Override it with a stat call to make an
    /// unchanged manifest cost one stat per request instead of one read. A stamp must differ
    /// whenever the content differs, so a coarse modification time is not enough on its own: pair
    /// it with the size.
    ///
    /// @param path File to fingerprint.
    /// @return The stamp, or std::nullopt when it is unknown or the file does not exist.
    [[nodiscard]] virtual std::optional<FileStamp> stamp(const std::filesystem::path &path) const {
        static_cast<void>(path);
        return std::nullopt;
    }
};

} // namespace inertia
