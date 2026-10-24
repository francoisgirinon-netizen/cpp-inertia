#pragma once

#include <inertia/ports/file_reader.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace inertia {

/// Vite layout of the front end, relative to the file reader's root.
struct ViteConfig {
    /// Public directory. The hot file and the build directory are read below it.
    std::filesystem::path publicDir = "public";
    /// Build output directory below publicDir. The manifest is `.vite/manifest.json` inside it.
    std::string buildDir = "build";
    /// Name of the file, inside publicDir, that the Vite dev server writes while it runs. Its
    /// content is the dev server URL.
    std::string hotFile = "hot";
    /// Entry modules of the front end, such as `src/app.tsx`. They are keys of the manifest.
    std::vector<std::string> entries{"src/app.tsx"};
    /// Emits the React Fast Refresh preamble in dev mode. Disable it for non-React front ends.
    bool reactRefresh = true;
};

/// Renders the `<script>` and `<link>` tags that load the front end.
///
/// While the hot file exists, the tags point at the Vite dev server. Otherwise they come from the
/// production build listed in the Vite manifest. Files are read through the injected FileReader.
class Vite {
  public:
    /// Creates the Vite integration.
    /// @param cfg Layout of the front end.
    /// @param files Reader for the hot file and the manifest.
    Vite(ViteConfig cfg, std::shared_ptr<const FileReader> files);

    /// Reads the dev server URL from the hot file.
    /// @return The URL, trimmed, without trailing slashes. Nullopt when the hot file is missing
    /// or blank.
    [[nodiscard]] std::optional<std::string> hotUrl() const;

    /// Renders the tags for the entries.
    /// @return Dev-server tags when the hot file exists, otherwise production tags from the
    /// manifest. Empty when neither is available. A missing or malformed manifest never throws.
    [[nodiscard]] std::string tags() const;

    /// Hashes the manifest, to derive an asset version that changes when the build changes.
    /// @return The FNV-1a 64-bit hash of the raw manifest as 16 lowercase hex digits. Empty when
    /// the manifest is missing or is not a JSON object.
    [[nodiscard]] std::string manifestHash() const;

  private:
    /// Dev-server tags that load the entries from `url`.
    [[nodiscard]] std::string devTags(std::string_view url) const;
    /// Production tags built from the manifest.
    [[nodiscard]] std::string prodTags() const;
    /// Raw manifest content, if the file can be read.
    [[nodiscard]] std::optional<std::string> manifestContent() const;

    ViteConfig cfg_;
    std::shared_ptr<const FileReader> files_;
};

/// Builds an asset-version callback for Config::version from a Vite integration. The version
/// changes whenever the built assets change, so clients reload after a deployment.
/// @param vite Vite integration to hash. Null yields an empty version.
/// @return A callable that returns Vite::manifestHash().
[[nodiscard]] std::function<std::string()> versionFromVite(std::shared_ptr<const Vite> vite);

} // namespace inertia
