#pragma once

#include <inertia/json.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace inertia {

// Serializes compactly; invalid UTF-8 is replaced by U+FFFD and never throws.
[[nodiscard]] std::string dumpJson(const Json &j);

// dumpJson, then "/" -> "\/", "<" -> "<", ">" -> ">" (and U+2028/U+2029 escaped),
// so the result is safe inside a script element.
[[nodiscard]] std::string escapeForScript(const Json &page);

// <script data-page="ROOT" type="application/json">JSON</script><div id="ROOT"></div>
[[nodiscard]] std::string inertiaBody(const Json &page, std::string_view rootId);

struct RootContext {
    // Custom root templates read the page with ctx.page.get().
    std::reference_wrapper<const Json> page; // non-owning view, short-lived
    std::string head;
    std::string body;
    std::string viteTags;
};

using RootTemplate = std::function<std::string(const RootContext &)>;

[[nodiscard]] std::string defaultRootTemplate(const RootContext &ctx);

} // namespace inertia
