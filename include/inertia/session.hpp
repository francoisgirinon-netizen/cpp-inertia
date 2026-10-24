#pragma once

#include <inertia/json.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/response.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

/// Builds a redirect response.
/// @param url Location to redirect to.
/// @param status Redirect status code. 302 by default.
/// @return The redirect response.
[[nodiscard]] Response redirect(std::string url, int status = 302);

/// Builds a 302 redirect to the `Referer` header, or to `/` when the request has none.
/// @param req Request being served.
/// @return The redirect response.
[[nodiscard]] Response back(const RequestView &req);

/// Validation errors as ordered pairs of field name and messages.
using Errors = std::vector<std::pair<std::string, std::vector<std::string>>>;

/// Stores validation errors for the next page render, where they appear in the `errors` prop.
/// Fields already stored in the same bag are replaced.
/// @param session Session of the request that failed validation.
/// @param errors Messages for each field.
/// @param bag Name of the error bag, so that a page with several forms keeps their errors apart.
/// Defaults to `"default"`.
void withErrors(SessionStore &session, const Errors &errors, std::string_view bag = "default");

/// Stores a flash message for the next page render, where it appears in the `flash` object.
/// Calling it again with the same key replaces the value.
/// @param session Session of the request.
/// @param key Name of the flash message.
/// @param value Value of the flash message.
void flash(SessionStore &session, std::string key, Json value);

/// Asks the client to clear its history state on the next page render, such as after logout.
/// The `clearHistory` flag is sent once.
/// @param session Session of the request.
void clearHistory(SessionStore &session);

/// Asks the client to keep the URL fragment on the next page render. The `preserveFragment` flag
/// is sent once.
/// @param session Session of the request.
void preserveFragment(SessionStore &session);

} // namespace inertia
