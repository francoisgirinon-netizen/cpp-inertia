#include <inertia/drogon/drogon.hpp>
#include <inertia/json.hpp>

#include <drogon/Session.h>

#include <any>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace inertia::drogon {

namespace {

// Corrupt or foreign values read as absent rather than throwing into a request handler.
std::optional<Json> parse(const std::string &text) {
    auto json = Json::parse(text, nullptr, false);
    if (json.is_discarded()) {
        return std::nullopt;
    }
    // In place: converting a Json to an optional<Json> is ambiguous with nlohmann's conversion
    // operator for GCC.
    return std::optional<Json>{std::in_place, std::move(json)};
}

} // namespace

DrogonSession::DrogonSession(::drogon::SessionPtr session) : session_(std::move(session)) {
    if (!session_) {
        throw std::invalid_argument(
            "DrogonSession needs a session: req->session() is null, call app().enableSession()");
    }
}

std::optional<Json> DrogonSession::get(std::string_view key) const {
    const auto text = session_->getOptional<std::string>(std::string{key});
    return text ? parse(*text) : std::nullopt;
}

void DrogonSession::put(std::string key, Json value) {
    // modify() replaces under the session lock; insert() would keep an existing value.
    session_->modify<std::string>(key,
                                  [text = value.dump()](std::string &stored) { stored = text; });
}

std::optional<Json> DrogonSession::pull(std::string_view key) {
    std::optional<std::string> text;
    session_->modify([&](::drogon::Session::SessionMap &all) {
        const auto it = all.find(std::string{key});
        if (it == all.end()) {
            return;
        }
        if (const auto *stored = std::any_cast<std::string>(&it->second)) {
            text = *stored;
        }
        all.erase(it);
    });
    return text ? parse(*text) : std::nullopt;
}

} // namespace inertia::drogon
