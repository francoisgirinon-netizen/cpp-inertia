#include <inertia/html.hpp>

#include <string>
#include <string_view>

namespace inertia {

std::string dumpJson(const Json &j) {
    return j.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string escapeForScript(const Json &page) {
    const std::string raw = dumpJson(page);
    std::string out;
    out.reserve(raw.size() + raw.size() / 8);
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c == '/') {
            out += "\\/";
        } else if (c == '<') {
            out += "\\u003C";
        } else if (c == '>') {
            out += "\\u003E";
        } else if (c == '\xE2' && i + 2 < raw.size() && raw[i + 1] == '\x80' &&
                   (raw[i + 2] == '\xA8' || raw[i + 2] == '\xA9')) {
            out += raw[i + 2] == '\xA8' ? "\\u2028" : "\\u2029";
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

std::string inertiaBody(const Json &page, std::string_view rootId) {
    std::string out = R"(<script data-page=")";
    out += rootId;
    out += R"(" type="application/json">)";
    out += escapeForScript(page);
    out += R"(</script><div id=")";
    out += rootId;
    out += R"("></div>)";
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
