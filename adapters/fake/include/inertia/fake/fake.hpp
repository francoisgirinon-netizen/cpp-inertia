#pragma once

#include <inertia/attributes.hpp>
#include <inertia/fake/export.hpp>
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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// @brief In-memory adapter implementations for unit tests.
///
/// Every type here is a plain test double: no sockets, no files, no clock. Use them in place of the
/// production adapters to drive the core without a network.
namespace inertia::fake {

/// @brief Request double implementing RequestView.
///
/// Build one with the static factories and chain with() for headers. The public fields can also be
/// set directly.
///
/// @code{.cpp}
/// auto request = inertia::fake::FakeRequest::inertia("/users?page=2", "v1")
///                    .with("Accept", "application/json");
/// @endcode
INERTIA_DLL_INTERFACE_BEGIN
struct INERTIA_FAKE_EXPORT FakeRequest final : RequestView {
    /// Request method, such as "GET" or "POST".
    std::string verb = "GET";
    /// Request target: the path, with the query string if any.
    std::string path = "/";
    /// Scheme and host that fullUrl() prefixes to path.
    std::string origin = "http://localhost";
    /// Header lines in insertion order, as (name, value) pairs.
    std::vector<std::pair<std::string, std::string>> hdrs;

    /// Sets a header, replacing any existing header with the same name (case-insensitive).
    /// @param name The header name.
    /// @param value The header value.
    /// @return This request, for chaining.
    FakeRequest &with(std::string name, std::string value);
    /// A GET request for `url` with no headers.
    /// @param url The request target, including the query string if any.
    /// @return The request.
    static FakeRequest get(std::string url);
    /// A GET request that looks like an Inertia XHR: it always carries X-Inertia: true and
    /// X-Inertia-Version, even when `version` is empty.
    /// @param url The request target, including the query string if any.
    /// @param version The asset version the client reports.
    /// @return The request.
    static FakeRequest inertia(std::string url, std::string version = "");

    /// @return `verb`.
    [[nodiscard]] std::string_view method() const override;
    /// @return `path`.
    [[nodiscard]] std::string url() const override;
    /// @return `origin` followed by `path`.
    [[nodiscard]] std::string fullUrl() const override;
    /// Looks up a header by name.
    /// @param name The header name, matched case-insensitively.
    /// @return The first matching value, or std::nullopt when absent.
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const override;
};
INERTIA_DLL_INTERFACE_END

/// @brief Session store double backed by an ordered map.
///
/// Not thread safe.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_FAKE_EXPORT MemorySession final : public SessionStore {
  public:
    /// @param key The key to look up.
    /// @return The stored value, or std::nullopt when `key` is absent.
    [[nodiscard]] std::optional<Json> get(std::string_view key) const override;
    /// Stores `value` under `key`, replacing any previous value.
    /// @param key The key to store under.
    /// @param value The value to store.
    void put(std::string key, Json value) override;
    /// Removes `key` from the session.
    /// @param key The key to remove.
    /// @return The removed value, or std::nullopt when `key` was absent.
    std::optional<Json> pull(std::string_view key) override;

  private:
    std::map<std::string, Json, std::less<>> data_;
};
INERTIA_DLL_INTERFACE_END

/// @brief HTTP client double that replays queued replies.
///
/// Replies are consumed in order; the callback is invoked synchronously. When no reply is queued,
/// the callback receives status 0 with an error set. The timeout is ignored. Not thread safe.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_FAKE_EXPORT FakeHttpClient final : public HttpClient {
  public:
    /// Queued replies, consumed from the front by get() and post(). Push to it to script answers.
    std::deque<HttpResult> replies;
    /// The (url, jsonBody) pair of every post() call, in call order.
    std::vector<std::pair<std::string, std::string>> posts;
    /// The url of every get() call, in call order.
    std::vector<std::string> gets;

    /// Records the call and answers with the next queued reply.
    /// @param url The URL the call targets; recorded in `posts`.
    /// @param jsonBody The body the call sends; recorded in `posts`.
    /// @param timeout Ignored.
    /// @param done Invoked synchronously, before this function returns.
    void post(std::string url, std::string jsonBody, std::chrono::milliseconds timeout,
              std::function<void(HttpResult)> done) override;
    /// Records the call and answers with the next queued reply.
    /// @param url The URL the call targets; recorded in `gets`.
    /// @param timeout Ignored.
    /// @param done Invoked synchronously, before this function returns.
    void get(std::string url, std::chrono::milliseconds timeout,
             std::function<void(HttpResult)> done) override;

  private:
    HttpResult nextReply();
};
INERTIA_DLL_INTERFACE_END

/// @brief File reader double backed by an in-memory map.
INERTIA_DLL_INTERFACE_BEGIN
class INERTIA_FAKE_EXPORT MemoryFileReader final : public FileReader {
  public:
    /// Contents by path. Only exact path matches are found; paths are not normalized.
    std::map<std::filesystem::path, std::string> files;

    /// @param path The path to read, matched exactly against `files`.
    /// @return The stored content, or std::nullopt when `path` is not in `files`.
    [[nodiscard]] std::optional<std::string> read(const std::filesystem::path &path) const override;

    /// Fingerprints the stored content, so that code that caches by stamp sees every edit of
    /// `files`, including one that keeps the size.
    /// @param path The path to stamp, matched exactly against `files`.
    /// @return The size and a hash of the content, or std::nullopt when `path` is not in `files`.
    [[nodiscard]] std::optional<FileStamp> stamp(const std::filesystem::path &path) const override;
};
INERTIA_DLL_INTERFACE_END

} // namespace inertia::fake
