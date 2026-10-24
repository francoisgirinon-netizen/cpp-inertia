#pragma once

#include <inertia/response.hpp>

#include <drogon/HttpResponse.h>

namespace inertia::drogon::detail {

// The status and the headers of a Drogon response as an Inertia response, in the order fromDrogon()
// reads them (Content-Type first, then the headers, then each cookie as a Set-Cookie header). The
// body is left empty, for callers that must not pay for a copy of it.
[[nodiscard]] Response responseHead(const ::drogon::HttpResponsePtr &res);

} // namespace inertia::drogon::detail
