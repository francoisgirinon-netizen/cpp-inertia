#pragma once

#include <inertia/ports/file_reader.hpp>

#include <filesystem>
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

    // Dev tags when the hot file exists, else production tags; "" when neither is available.
    [[nodiscard]] std::string tags() const;

  private:
    [[nodiscard]] std::string devTags(const std::string &url) const;

    ViteConfig cfg_;
    std::shared_ptr<const FileReader> files_;
};

} // namespace inertia
