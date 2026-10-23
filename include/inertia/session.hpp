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

// A redirect response to url with the given status (302 by default).
[[nodiscard]] Response redirect(std::string url, int status = 302);

// A 302 redirect to the Referer header, or to "/" when the request has none.
[[nodiscard]] Response back(const RequestView &req);

// field -> messages
using Errors = std::vector<std::pair<std::string, std::vector<std::string>>>;

// Stores validation errors under "inertia.errors" ({bag: {field: [messages]}}), merging into the
// bag; they are consumed by the next page render.
void withErrors(SessionStore &session, const Errors &errors, std::string_view bag = "default");

} // namespace inertia
