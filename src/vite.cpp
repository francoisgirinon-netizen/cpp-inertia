#include <inertia/vite.hpp>

#include <inertia/json.hpp>
#include <inertia/ports/file_reader.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

namespace {

constexpr std::string_view kWhitespace = " \t\r\n";

// FNV-1a 64-bit hash of the raw bytes.
std::uint64_t fnv1a64(std::string_view data) {
    constexpr std::uint64_t kOffsetBasis = 0xcbf29ce484222325ULL;
    constexpr std::uint64_t kPrime = 0x100000001b3ULL;
    std::uint64_t hash = kOffsetBasis;
    for (const auto byte : data) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= kPrime;
    }
    return hash;
}

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

// Appends a stylesheet link unless that stylesheet was already emitted.
void appendStylesheet(const std::string &buildDir, const std::string &css,
                      std::set<std::string> &seenCss, std::string &out) {
    if (seenCss.insert(css).second) {
        out += R"(<link rel="stylesheet" href=")" + assetUrl(buildDir, css) + R"(">)";
    }
}

// Appends the stylesheet and modulepreload tags of every chunk reachable through imports, once
// each.
void appendImports(const Json &manifest, const std::vector<std::string> &imports,
                   const std::string &buildDir, std::set<std::string> &seen,
                   std::set<std::string> &seenCss, std::string &out) {
    for (const auto &import : imports) {
        if (!seen.insert(import).second) {
            continue;
        }
        const auto found = manifest.find(import);
        if (found == manifest.end() || !found->is_object()) {
            continue;
        }
        for (const auto &css : stringList(*found, "css")) {
            appendStylesheet(buildDir, css, seenCss, out);
        }
        if (const auto file = stringValue(*found, "file")) {
            out += R"(<link rel="modulepreload" href=")" + assetUrl(buildDir, *file) + R"(">)";
        }
        appendImports(manifest, stringList(*found, "imports"), buildDir, seen, seenCss, out);
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

// What is derived from one version of the manifest file.
struct Vite::Snapshot {
    // Stamp of the file when it was read; nullopt when the reader cannot stamp it.
    std::optional<FileStamp> stamp;
    // The raw content, to recognize an unchanged file whose stamp is unknown or has moved.
    std::string content;
    // manifestHash(): empty when the content is not a JSON object.
    std::string hash;
    // The production tags: empty when the content is not a JSON object.
    std::string tags;
};

// Shared by the copies of a Vite. The snapshot is immutable and swapped atomically, so request
// threads never wait for each other and never see a half-built one.
struct Vite::Cache {
    std::atomic<std::shared_ptr<const Snapshot>> current;
};

Vite::Vite(ViteConfig cfg, std::shared_ptr<const FileReader> files)
    : cfg_(std::move(cfg)), files_(std::move(files)),
      manifestPath_(cfg_.publicDir / cfg_.buildDir / ".vite" / "manifest.json"),
      cache_(std::make_shared<Cache>()) {}

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
    if (end == std::string::npos || end < begin) {
        return std::nullopt; // nothing but slashes and whitespace
    }
    return content->substr(begin, end - begin + 1);
}

std::string Vite::tags() const {
    if (const auto url = hotUrl()) {
        return devTags(*url);
    }
    return snapshot()->tags;
}

std::string Vite::devTags(std::string_view url) const {
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

std::shared_ptr<const Vite::Snapshot> Vite::snapshot() const {
    auto cached = cache_->current.load();
    // The stamp is taken before the content: a file that changes in between leaves an old stamp
    // on newer content, which only costs one more read, never a stale hit.
    const auto stamp = files_->stamp(manifestPath_);
    if (cached && stamp && cached->stamp == stamp) {
        return cached;
    }
    auto content = files_->read(manifestPath_);
    if (!content) {
        static const auto missing = std::make_shared<const Snapshot>();
        return missing; // not cached: the file may come back
    }
    if (cached && cached->content == *content) {
        if (cached->stamp == stamp) {
            return cached; // a reader without stamps: same content, nothing to refresh
        }
        auto touched = std::make_shared<Snapshot>(*cached);
        touched->stamp = stamp;
        cached = std::move(touched);
        cache_->current.store(cached);
        return cached;
    }
    auto fresh = std::make_shared<Snapshot>();
    fresh->stamp = stamp;
    if (const auto manifest = parseManifest(content)) {
        fresh->hash = std::format("{:016x}", fnv1a64(*content));
        fresh->tags = prodTags(*manifest);
    }
    fresh->content = std::move(*content);
    cache_->current.store(fresh);
    return fresh;
}

std::string Vite::prodTags(const Json &manifest) const {
    // Entries are seen up front so an entry imported by another is not preloaded.
    std::set<std::string> seen(cfg_.entries.begin(), cfg_.entries.end());
    std::set<std::string> seenCss;
    std::string out;
    for (const auto &entry : cfg_.entries) {
        const auto found = manifest.find(entry);
        if (found == manifest.end() || !found->is_object()) {
            continue;
        }
        const auto file = stringValue(*found, "file");
        if (!file) {
            continue;
        }
        for (const auto &css : stringList(*found, "css")) {
            appendStylesheet(cfg_.buildDir, css, seenCss, out);
        }
        appendImports(manifest, stringList(*found, "imports"), cfg_.buildDir, seen, seenCss, out);
        out += R"(<script type="module" src=")" + assetUrl(cfg_.buildDir, *file) + R"("></script>)";
    }
    return out;
}

std::string Vite::manifestHash() const { return snapshot()->hash; }

std::function<std::string()> versionFromVite(std::shared_ptr<const Vite> vite) {
    return [held = std::move(vite)] { return held ? held->manifestHash() : std::string{}; };
}

} // namespace inertia
