#include <inertia/json.hpp>
#include <inertia/vite.hpp>

#include <set>
#include <string>
#include <utility>

namespace inertia {

namespace {

constexpr std::string_view kWhitespace = " \t\r\n";

std::string assetUrl(const std::string &buildDir, const std::string &file) {
    return "/" + buildDir + "/" + file;
}

// The string items of chunk[key], or nothing when the key is absent or not an array.
std::vector<std::string> stringList(const Json &chunk, const char *key) {
    std::vector<std::string> out;
    const auto found = chunk.find(key);
    if (found == chunk.end() || !found->is_array()) {
        return out;
    }
    for (const auto &item : *found) {
        if (item.is_string()) {
            out.push_back(item.get<std::string>());
        }
    }
    return out;
}

// The string value of chunk[key], or nullopt.
std::optional<std::string> stringValue(const Json &chunk, const char *key) {
    const auto found = chunk.find(key);
    if (found == chunk.end() || !found->is_string()) {
        return std::nullopt;
    }
    return found->get<std::string>();
}

// Appends the stylesheet and modulepreload tags of every chunk reachable through imports, once
// each.
void appendImports(const Json &manifest, const std::vector<std::string> &imports,
                   const std::string &buildDir, std::set<std::string> &seen, std::string &out) {
    for (const auto &import : imports) {
        if (!seen.insert(import).second) {
            continue;
        }
        const auto found = manifest.find(import);
        if (found == manifest.end() || !found->is_object()) {
            continue;
        }
        for (const auto &css : stringList(*found, "css")) {
            out += R"(<link rel="stylesheet" href=")" + assetUrl(buildDir, css) + R"(">)";
        }
        if (const auto file = stringValue(*found, "file")) {
            out += R"(<link rel="modulepreload" href=")" + assetUrl(buildDir, *file) + R"(">)";
        }
        appendImports(manifest, stringList(*found, "imports"), buildDir, seen, out);
    }
}

// Parses the manifest; malformed JSON or a non-object root is reported as nullopt.
std::optional<Json> parseManifest(const std::optional<std::string> &content) {
    if (!content) {
        return std::nullopt;
    }
    Json manifest = Json::parse(*content, nullptr, false);
    if (manifest.is_discarded() || !manifest.is_object()) {
        return std::nullopt;
    }
    return manifest;
}

} // namespace

Vite::Vite(ViteConfig cfg, std::shared_ptr<const FileReader> files)
    : cfg_(std::move(cfg)), files_(std::move(files)) {}

std::optional<std::string> Vite::hotUrl() const {
    const auto content = files_->read(cfg_.publicDir / cfg_.hotFile);
    if (!content) {
        return std::nullopt;
    }
    const auto begin = content->find_first_not_of(kWhitespace);
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    const auto end = content->find_last_not_of(std::string(kWhitespace) + "/");
    return content->substr(begin, end - begin + 1);
}

std::string Vite::tags() const {
    if (const auto url = hotUrl()) {
        return devTags(*url);
    }
    return prodTags();
}

std::string Vite::devTags(const std::string &url) const {
    std::string out;
    if (cfg_.reactRefresh) {
        out += R"V(<script type="module">import RefreshRuntime from ')V";
        out += url;
        out +=
            R"V(/@react-refresh';RefreshRuntime.injectIntoGlobalHook(window);window.$RefreshReg$ = () => {};window.$RefreshSig$ = () => (type) => type;window.__vite_plugin_react_preamble_installed__ = true;</script>)V";
    }
    out += R"V(<script type="module" src=")V";
    out += url;
    out += R"V(/@vite/client"></script>)V";
    for (const auto &entry : cfg_.entries) {
        out += R"V(<script type="module" src=")V";
        out += url;
        out += '/';
        out += entry;
        out += R"V("></script>)V";
    }
    return out;
}

std::string Vite::prodTags() const {
    const auto manifest = parseManifest(manifestContent());
    if (!manifest) {
        return {};
    }
    // Entries are seen up front so an entry imported by another is not preloaded.
    std::set<std::string> seen(cfg_.entries.begin(), cfg_.entries.end());
    std::string out;
    for (const auto &entry : cfg_.entries) {
        const auto found = manifest->find(entry);
        if (found == manifest->end() || !found->is_object()) {
            continue;
        }
        const auto file = stringValue(*found, "file");
        if (!file) {
            continue;
        }
        for (const auto &css : stringList(*found, "css")) {
            out += R"(<link rel="stylesheet" href=")" + assetUrl(cfg_.buildDir, css) + R"(">)";
        }
        appendImports(*manifest, stringList(*found, "imports"), cfg_.buildDir, seen, out);
        out += R"(<script type="module" src=")" + assetUrl(cfg_.buildDir, *file) + R"("></script>)";
    }
    return out;
}

std::optional<std::string> Vite::manifestContent() const {
    return files_->read(cfg_.publicDir / cfg_.buildDir / ".vite" / "manifest.json");
}

} // namespace inertia
