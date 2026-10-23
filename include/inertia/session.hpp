#pragma once

#include <inertia/ports/request_view.hpp>
#include <inertia/response.hpp>

#include <string>

namespace inertia {

// A redirect response to url with the given status (302 by default).
[[nodiscard]] Response redirect(std::string url, int status = 302);

// A 302 redirect to the Referer header, or to "/" when the request has none.
[[nodiscard]] Response back(const RequestView &req);

} // namespace inertia
