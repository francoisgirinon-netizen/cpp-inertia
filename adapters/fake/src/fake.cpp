#include <inertia/fake/fake.hpp>

#include <inertia/headers.hpp>

#include <optional>
#include <utility>

namespace inertia::fake {

FakeRequest &FakeRequest::with(std::string name, std::string value) {
    std::erase_if(hdrs, [&](const auto &entry) { return headers::iequals(entry.first, name); });
    hdrs.emplace_back(std::move(name), std::move(value));
    return *this;
}

FakeRequest FakeRequest::get(std::string url) {
    FakeRequest request;
    request.path = std::move(url);
    return request;
}

FakeRequest FakeRequest::inertia(std::string url, std::string version) {
    FakeRequest request = get(std::move(url));
    request.with(std::string{headers::kInertia}, "true");
    request.with(std::string{headers::kVersion}, std::move(version));
    return request;
}

std::string_view FakeRequest::method() const { return verb; }

std::string FakeRequest::url() const { return path; }

std::string FakeRequest::fullUrl() const { return origin + path; }

std::optional<std::string> FakeRequest::header(std::string_view name) const {
    for (const auto &[key, value] : hdrs) {
        if (headers::iequals(key, name)) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<Json> MemorySession::get(std::string_view key) const {
    const auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    return std::optional<Json>{std::in_place, it->second};
}

void MemorySession::put(std::string key, Json value) {
    data_.insert_or_assign(std::move(key), std::move(value));
}

std::optional<Json> MemorySession::pull(std::string_view key) {
    const auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    Json value = std::move(it->second);
    data_.erase(it);
    return std::optional<Json>{std::in_place, std::move(value)};
}

void FakeHttpClient::post(std::string url, std::string jsonBody,
                          std::chrono::milliseconds /*timeout*/,
                          std::function<void(HttpResult)> done) {
    posts.emplace_back(std::move(url), std::move(jsonBody));
    done(nextReply());
}

void FakeHttpClient::get(std::string url, std::chrono::milliseconds /*timeout*/,
                         std::function<void(HttpResult)> done) {
    gets.push_back(std::move(url));
    done(nextReply());
}

HttpResult FakeHttpClient::nextReply() {
    if (replies.empty()) {
        HttpResult missing;
        missing.error = "no reply queued";
        return missing;
    }
    HttpResult reply = std::move(replies.front());
    replies.pop_front();
    return reply;
}

std::optional<std::string> MemoryFileReader::read(const std::filesystem::path &path) const {
    const auto it = files.find(path);
    if (it == files.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace inertia::fake
