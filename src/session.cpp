#include <inertia/session.hpp>

#include <inertia/headers.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace inertia {

namespace {
constexpr std::string_view kErrorsKey = "inertia.errors";
constexpr std::string_view kFlashKey = "inertia.flash_data";
} // namespace

Response redirect(std::string url, int status) {
    Response res;
    res.status = status;
    res.setHeader("Location", std::move(url));
    return res;
}

Response back(const RequestView &req) { return redirect(req.header("Referer").value_or("/")); }

void withErrors(SessionStore &session, const Errors &errors, std::string_view bag) {
    Json all = session.get(kErrorsKey).value_or(Json::object());
    if (!all.is_object()) {
        all = Json::object();
    }
    Json &target = all[std::string(bag)];
    if (!target.is_object()) {
        target = Json::object();
    }
    for (const auto &[field, messages] : errors) {
        target[field] = messages;
    }
    session.put(std::string(kErrorsKey), std::move(all));
}

void flash(SessionStore &session, std::string key, Json value) {
    Json data = session.get(kFlashKey).value_or(Json::object());
    if (!data.is_object()) {
        data = Json::object();
    }
    data[std::move(key)] = std::move(value);
    session.put(std::string(kFlashKey), std::move(data));
}

} // namespace inertia
