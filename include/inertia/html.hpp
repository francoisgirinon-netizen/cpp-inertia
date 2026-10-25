#pragma once

#include <inertia/core_export.hpp>
#include <inertia/json.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace inertia {

/// Serializes a JSON value compactly. Invalid UTF-8 is replaced by U+FFFD, so this never throws.
/// @param j Value to serialize.
/// @return The compact JSON text.
[[nodiscard]] INERTIA_CORE_EXPORT std::string dumpJson(const Json &j);

/// Serializes a page for embedding in a `<script>` element. Besides dumpJson(), it escapes `/`,
/// `<`, `>`, U+2028 and U+2029, so the text cannot close the element early.
/// @param page Page object to serialize.
/// @return The escaped JSON text.
[[nodiscard]] INERTIA_CORE_EXPORT std::string escapeForScript(const Json &page);

/// Builds the body markup of a page: the page JSON in a `<script data-page>` element and an empty
/// root element that the front end mounts on.
/// @param page Page object to embed.
/// @param rootId Id of the root element, which also names the `data-page` attribute.
/// @return The markup, as
/// `<script data-page="ROOT" type="application/json">JSON</script><div id="ROOT"></div>`.
[[nodiscard]] INERTIA_CORE_EXPORT std::string inertiaBody(const Json &page,
                                                          std::string_view rootId);

/// Inputs to a RootTemplate, which builds the HTML document of a first visit.
struct RootContext {
    /// The page object, as a non-owning view. Read it with `ctx.page.get()`. The reference is
    /// valid only during the template call.
    std::reference_wrapper<const Json> page;
    /// Head markup from SSR. Empty without SSR.
    std::string head;
    /// Body markup: the SSR output, or the inertiaBody() markup when SSR is off or failed.
    std::string body;
    /// Vite script and link tags for the entries. Empty without Vite.
    std::string viteTags;
};

/// Builds the complete HTML document of a first visit from its context.
using RootTemplate = std::function<std::string(const RootContext &)>;

/// Builds an HTML5 document with the Vite tags and head markup in `<head>`, and the body markup
/// in `<body>`.
/// @param ctx Inputs of the document.
/// @return The complete document.
[[nodiscard]] INERTIA_CORE_EXPORT std::string defaultRootTemplate(const RootContext &ctx);

} // namespace inertia
