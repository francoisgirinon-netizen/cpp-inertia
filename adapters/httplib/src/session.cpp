#include <inertia/httplib/httplib.hpp>

#include "random.hpp"

#include <algorithm>
#include <cstddef>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace inertia::httplib {

class SessionData {
  public:
    std::optional<Json> get(std::string_view key) const {
        const std::scoped_lock lock{mutex_};
        const auto it = values_.find(key);
        return it == values_.end() ? std::nullopt : std::optional<Json>{it->second};
    }

    void put(std::string key, Json value) {
        const std::scoped_lock lock{mutex_};
        values_.insert_or_assign(std::move(key), std::move(value));
    }

    std::optional<Json> pull(std::string_view key) {
        const std::scoped_lock lock{mutex_};
        const auto it = values_.find(key);
        if (it == values_.end()) {
            return std::nullopt;
        }
        std::optional<Json> value{std::move(it->second)};
        values_.erase(it);
        return value;
    }

    Json getOrCreate(std::string_view key, const std::function<Json()> &make) {
        const std::scoped_lock lock{mutex_};
        auto it = values_.find(key);
        if (it == values_.end()) {
            it = values_.emplace(std::string{key}, make()).first;
        }
        return it->second;
    }

  private:
    mutable std::mutex mutex_;
    std::map<std::string, Json, std::less<>> values_; // guarded by mutex_
};

namespace {

constexpr std::size_t kIdBytes = 32;
constexpr std::size_t kIdLength = kIdBytes * 2;

} // namespace

MemorySessionStore::MemorySessionStore(MemorySessionOptions options)
    : options_(std::move(options)) {
    options_.maxSessions = std::max<std::size_t>(options_.maxSessions, 1);
}

MemorySessionStore::~MemorySessionStore() = default;

std::chrono::steady_clock::time_point MemorySessionStore::now() const {
    return options_.clock ? options_.clock() : std::chrono::steady_clock::now();
}

// The list is ordered by last use, so the expired sessions are exactly its tail.
void MemorySessionStore::expireLocked(std::chrono::steady_clock::time_point at) {
    while (!lru_.empty()) {
        const auto it = sessions_.find(lru_.back());
        if (at - it->second.lastUsed <= options_.idleTtl) {
            break;
        }
        sessions_.erase(it);
        lru_.pop_back();
    }
}

MemorySessionStore::Opened
MemorySessionStore::insertLocked(std::string id, std::shared_ptr<SessionData> data,
                                 std::chrono::steady_clock::time_point at) {
    while (sessions_.size() >= options_.maxSessions && !lru_.empty()) {
        sessions_.erase(lru_.back());
        lru_.pop_back();
    }
    lru_.push_front(id);
    sessions_.emplace(id, Entry{.data = data, .lastUsed = at, .position = lru_.begin()});
    return {.id = std::move(id), .data = std::move(data), .created = true};
}

void MemorySessionStore::sweep() {
    const auto at = now();
    const std::scoped_lock lock{mutex_};
    expireLocked(at);
}

std::size_t MemorySessionStore::size() const {
    const std::scoped_lock lock{mutex_};
    return sessions_.size();
}

MemorySessionStore::Opened MemorySessionStore::open(std::optional<std::string_view> id) {
    const auto at = now();
    // Generated before taking the lock: it may throw, and the syscall needs no protection.
    std::string fresh = detail::randomHex(kIdBytes);

    const std::scoped_lock lock{mutex_};
    expireLocked(at);
    if (id && id->size() == kIdLength) {
        if (const auto it = sessions_.find(std::string{*id}); it != sessions_.end()) {
            it->second.lastUsed = at;
            lru_.splice(lru_.begin(), lru_, it->second.position);
            return {.id = it->first, .data = it->second.data, .created = false};
        }
    }
    return insertLocked(std::move(fresh), std::make_shared<SessionData>(), at);
}

MemorySessionStore::Opened MemorySessionStore::rotate(std::string_view oldId) {
    const auto at = now();
    std::string fresh = detail::randomHex(kIdBytes);

    const std::scoped_lock lock{mutex_};
    expireLocked(at);
    std::shared_ptr<SessionData> data;
    if (const auto it = sessions_.find(std::string{oldId}); it != sessions_.end()) {
        data = it->second.data;
        lru_.erase(it->second.position);
        sessions_.erase(it);
    } else {
        data = std::make_shared<SessionData>();
    }
    return insertLocked(std::move(fresh), std::move(data), at);
}

std::string MemorySessionStore::cookieHeader(std::string_view id, bool secure) const {
    std::string out = options_.cookieName;
    out += '=';
    out += id;
    out += "; Path=/; HttpOnly; SameSite=Lax";
    if (secure) {
        out += "; Secure";
    }
    return out;
}

HttplibSession::HttplibSession(std::shared_ptr<SessionData> data) : data_(std::move(data)) {}

std::optional<Json> HttplibSession::get(std::string_view key) const { return data_->get(key); }

void HttplibSession::put(std::string key, Json value) {
    data_->put(std::move(key), std::move(value));
}

std::optional<Json> HttplibSession::pull(std::string_view key) { return data_->pull(key); }

Json HttplibSession::getOrCreate(std::string_view key, const std::function<Json()> &make) {
    return data_->getOrCreate(key, make);
}

} // namespace inertia::httplib
