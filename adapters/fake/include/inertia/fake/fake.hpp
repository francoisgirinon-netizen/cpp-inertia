#pragma once

#include <inertia/json.hpp>
#include <inertia/ports/file_reader.hpp>
#include <inertia/ports/http_client.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>

#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia::fake {

struct FakeRequest final : RequestView {
    std::string verb = "GET";
    std::string path = "/";
    std::string origin = "http://localhost";
    std::vector<std::pair<std::string, std::string>> hdrs;

    // Sets a header, replacing any existing header with the same name (case-insensitive).
    FakeRequest &with(std::string name, std::string value);
    static FakeRequest get(std::string url);
    // Always sets X-Inertia: true and X-Inertia-Version (even when version is empty).
    static FakeRequest inertia(std::string url, std::string version = "");

    [[nodiscard]] std::string_view method() const override;
    [[nodiscard]] std::string url() const override;
    [[nodiscard]] std::string fullUrl() const override;
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override;
};

class MemorySession final : public SessionStore {
  public:
    [[nodiscard]] std::optional<Json> get(std::string_view key) const override;
    void put(std::string key, Json value) override;
    std::optional<Json> pull(std::string_view key) override;

  private:
    std::map<std::string, Json, std::less<>> data_;
};

// Replies are consumed in order; the callback is invoked synchronously. When no reply
// is queued, the callback receives status 0 with an error set.
class FakeHttpClient final : public HttpClient {
  public:
    std::deque<HttpResult> replies;
    std::vector<std::pair<std::string, std::string>> posts;
    std::vector<std::string> gets;

    void post(std::string url, std::string jsonBody, std::chrono::milliseconds timeout,
              std::function<void(HttpResult)> done) override;
    void get(std::string url, std::chrono::milliseconds timeout,
             std::function<void(HttpResult)> done) override;

  private:
    HttpResult nextReply();
};

class MemoryFileReader final : public FileReader {
  public:
    std::map<std::filesystem::path, std::string> files;

    [[nodiscard]] std::optional<std::string> read(const std::filesystem::path &path) const override;
};

} // namespace inertia::fake
