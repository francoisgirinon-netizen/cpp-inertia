#include <inertia/html.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace inertia {

std::string dumpJson(const Json &j) {
    return j.dump(-1, ' ', false, Json::error_handler_t::replace);
}

namespace {

// The escape sequence for the text that starts at raw[i], or empty when raw[i] is kept as is.
// `length` receives how many bytes of raw the sequence replaces.
std::string_view escapeAt(std::string_view raw, std::size_t i, std::size_t &length) {
    length = 1;
    switch (raw[i]) {
    case '/':
        return "\\/";
    case '<':
        return "\\u003C";
    case '>':
        return "\\u003E";
    case '\xE2': // U+2028 and U+2029 are E2 80 A8 and E2 80 A9
        if (i + 2 < raw.size() && raw[i + 1] == '\x80' &&
            (raw[i + 2] == '\xA8' || raw[i + 2] == '\xA9')) {
            length = 3;
            return raw[i + 2] == '\xA8' ? "\\u2028" : "\\u2029";
        }
        return {};
    default:
        return {};
    }
}

// Appends raw to out with the characters that could close a <script> element escaped. The text
// between two escapes is appended in one piece.
void appendEscaped(std::string &out, std::string_view raw) {
    std::size_t keep = 0; // start of the pending run that is copied unchanged
    for (std::size_t i = 0; i < raw.size(); ++i) {
        std::size_t length = 1;
        const auto replacement = escapeAt(raw, i, length);
        if (replacement.empty()) {
            continue;
        }
        out.append(raw.substr(keep, i - keep));
        out.append(replacement);
        i += length - 1;
        keep = i + 1;
    }
    out.append(raw.substr(keep));
}

// Room for the escaped form of a dumped page: the escapes of a typical page add under 12 %.
std::size_t escapedCapacity(std::size_t rawSize) { return rawSize + rawSize / 8; }

} // namespace

std::string escapeForScript(const Json &page) {
    const std::string raw = dumpJson(page);
    std::string out;
    out.reserve(escapedCapacity(raw.size()));
    appendEscaped(out, raw);
    return out;
}

std::string inertiaBody(const Json &page, std::string_view rootId) {
    constexpr std::string_view kScriptOpen = R"(<script data-page=")";
    constexpr std::string_view kScriptClose = R"(" type="application/json">)";
    constexpr std::string_view kScriptEnd = R"(</script><div id=")";
    constexpr std::string_view kDivEnd = R"("></div>)";
    const std::string raw = dumpJson(page);
    std::string out;
    out.reserve(kScriptOpen.size() + kScriptClose.size() + kScriptEnd.size() + kDivEnd.size() +
                2 * rootId.size() + escapedCapacity(raw.size()));
    out += kScriptOpen;
    out += rootId;
    out += kScriptClose;
    appendEscaped(out, raw);
    out += kScriptEnd;
    out += rootId;
    out += kDivEnd;
    return out;
}

std::string defaultRootTemplate(const RootContext &ctx) {
    std::string out = R"(<!DOCTYPE html><html><head><meta charset="utf-8">)";
    out += ctx.viteTags;
    out += ctx.head;
    out += "</head><body>";
    out += ctx.body;
    out += "</body></html>";
    return out;
}

} // namespace inertia
