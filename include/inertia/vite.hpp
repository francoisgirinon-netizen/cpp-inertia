#pragma once

#include <inertia/ports/file_reader.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace inertia {

struct ViteConfig {
    std::filesystem::path publicDir = "public";
    std::string buildDir = "build";
    std::string hotFile = "hot";
    std::vector<std::string> entries{"src/app.tsx"};
    bool reactRefresh = true;
};

// Renders the <script>/<link> tags that load the front end: the Vite dev server while the hot
// file exists, otherwise the production build listed in the Vite manifest.
class Vite {
  public:
    Vite(ViteConfig cfg, std::shared_ptr<const FileReader> files);

    // Trimmed content of publicDir/hotFile without trailing slashes; nullopt when absent or blank.
    [[nodiscard]] std::optional<std::string> hotUrl() const;

    // Dev tags when the hot file exists, else production tags from the manifest. A missing or
    // malformed manifest yields "" and never throws.
    [[nodiscard]] std::string tags() const;

    // FNV-1a 64-bit of the raw manifest as 16 lowercase hex digits; "" when the manifest is
    // missing or is not a JSON object.
    [[nodiscard]] std::string manifestHash() const;

  private:
    [[nodiscard]] std::string devTags(const std::string &url) const;
    [[nodiscard]] std::string prodTags() const;
    [[nodiscard]] std::optional<std::string> manifestContent() const;

    ViteConfig cfg_;
    std::shared_ptr<const FileReader> files_;
};

// Asset version for Config::version: the manifest hash, so it changes with the built assets.
[[nodiscard]] std::function<std::string()> versionFromVite(std::shared_ptr<const Vite> vite);

} // namespace inertia
