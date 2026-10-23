#include <inertia/vite.hpp>

#include <string>
#include <utility>

namespace inertia {

namespace {

constexpr std::string_view kWhitespace = " \t\r\n";

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
    return {};
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

} // namespace inertia
