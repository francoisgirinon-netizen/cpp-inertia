#pragma once

#include <inertia/ports/file_reader.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace inertia {

/// A FileReader that reads the local file system, with std::filesystem only: no framework.
///
/// It is what most applications pass to Vite. Its stamp is the size and the modification time at
/// the file system's finest resolution (nanoseconds where the file system has them), so Vite reads
/// and parses the manifest only after `vite build` rewrites it, and an unchanged manifest costs one
/// `stat` per request.
///
/// A stamp cannot see an edit that keeps the size and lands within one tick of the file system's
/// clock after the previous one. A build tool that rewrites the manifest has changed its hashes
/// and so its size, or runs long after the previous build, so this does not matter for a build
/// output; a file edited by hand twice in a row within a tick, to the same length, can be missed
/// until the next change.
///
/// Relative paths are resolved against the working directory of the process, like any file open.
class DiskFileReader final : public FileReader {
  public:
    /// Reads the whole file.
    /// @param path File to read.
    /// @return The content, or std::nullopt when the file does not exist, is not a regular file or
    /// cannot be opened.
    [[nodiscard]] std::optional<std::string> read(const std::filesystem::path &path) const override;

    /// Fingerprints a file with one `stat`.
    /// @param path File to fingerprint.
    /// @return Its size and modification time (in nanoseconds since the epoch of the file
    /// system's clock), or std::nullopt when the file does not exist or is not a regular file.
    [[nodiscard]] std::optional<FileStamp> stamp(const std::filesystem::path &path) const override;
};

} // namespace inertia
