#pragma once

#include <inertia/core_export.hpp>
#include <inertia/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace inertia {

/// Key/value storage for the current visitor's session, with JSON values.
///
/// The core stores flash data, validation errors and history flags here, under keys starting with
/// "inertia.". Those entries persist from one request to the next, so a redirect-after-POST works:
/// the POST stores them and the following GET renders and consumes them. Keys the core does not
/// use must be left untouched.
///
/// A visitor's browser can send several requests at once, so each operation must be atomic on its
/// own. Implementations need not support concurrent use beyond that.
class INERTIA_CORE_EXPORT SessionStore {
  protected:
    SessionStore() = default;
    /// Copy-constructs a store in a derived class.
    SessionStore(const SessionStore &) = default;
    /// Copy-assigns a store in a derived class.
    ///
    /// @return `*this`.
    SessionStore &operator=(const SessionStore &) = default;
    /// Move-constructs a store in a derived class.
    SessionStore(SessionStore &&) = default;
    /// Move-assigns a store in a derived class.
    ///
    /// @return `*this`.
    SessionStore &operator=(SessionStore &&) = default;

  public:
    /// Virtual so that stores can be destroyed through a SessionStore pointer.
    virtual ~SessionStore() = default;

    /// Reads a value without changing the session.
    ///
    /// @param key Name of the entry.
    /// @return The stored value, or std::nullopt when the key is absent.
    [[nodiscard]] virtual std::optional<Json> get(std::string_view key) const = 0;

    /// Stores a value, replacing any previous value under the same key.
    ///
    /// @param key Name of the entry.
    /// @param value JSON value to store.
    virtual void put(std::string key, Json value) = 0;

    /// Reads a value and removes it, atomically: the value is consumed by exactly one caller.
    ///
    /// @param key Name of the entry.
    /// @return The value that was stored, or std::nullopt when the key was absent.
    virtual std::optional<Json> pull(std::string_view key) = 0;
};

} // namespace inertia
