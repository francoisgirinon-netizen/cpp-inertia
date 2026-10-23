#include <inertia/session.hpp>

#include <inertia/headers.hpp>

#include <string>
#include <utility>

namespace inertia {

Response redirect(std::string url, int status) {
    Response res;
    res.status = status;
    res.setHeader("Location", std::move(url));
    return res;
}

Response back(const RequestView &req) { return redirect(req.header("Referer").value_or("/")); }

} // namespace inertia
