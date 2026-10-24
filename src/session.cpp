#include <inertia/session.hpp>

#include <inertia/detail/session_keys.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace inertia {

namespace {
namespace keys = detail::session_keys;
} // namespace

Response redirect(std::string url, int status) {
    Response res;
    res.status = status;
    res.setHeader("Location", std::move(url));
    return res;
}

Response back(const RequestView &req) { return redirect(req.header("Referer").value_or("/")); }

void withErrors(SessionStore &session, const Errors &errors, std::string_view bag) {
    Json all = session.get(keys::kErrors).value_or(Json::object());
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
    session.put(std::string(keys::kErrors), std::move(all));
}

void flash(SessionStore &session, std::string key, Json value) {
    Json data = session.get(keys::kFlash).value_or(Json::object());
    if (!data.is_object()) {
        data = Json::object();
    }
    data[std::move(key)] = std::move(value);
    session.put(std::string(keys::kFlash), std::move(data));
}

void clearHistory(SessionStore &session) { session.put(std::string(keys::kClearHistory), true); }

void preserveFragment(SessionStore &session) {
    session.put(std::string(keys::kPreserveFragment), true);
}

} // namespace inertia
