#pragma once

#include <inertia/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace inertia {

class SessionStore {
  protected:
    SessionStore() = default;
    SessionStore(const SessionStore &) = default;
    SessionStore &operator=(const SessionStore &) = default;
    SessionStore(SessionStore &&) = default;
    SessionStore &operator=(SessionStore &&) = default;

  public:
    virtual ~SessionStore() = default;

    [[nodiscard]] virtual std::optional<Json> get(std::string_view key) const = 0;
    virtual void put(std::string key, Json value) = 0;
    virtual std::optional<Json> pull(std::string_view key) = 0; // get + erase
};

} // namespace inertia
