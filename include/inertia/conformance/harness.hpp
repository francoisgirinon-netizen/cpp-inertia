#pragma once

#include <inertia/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia::conformance {

struct TestRequest {
    std::string method = "GET";
    std::string url = "/"; // path and query
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

struct TestResponse {
    int status = 0;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    // Case-insensitive lookup of the first matching header.
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const;
    // The body parsed as JSON.
    [[nodiscard]] Json json() const;
};

// Performs one HTTP exchange against the application under test. Each adapter's harness
// implements it, so the scenarios run unchanged against every adapter.
//
// Contract: a session persists between calls, and `POST /__reset` starts a fresh one.
TestResponse send(const TestRequest &req);

} // namespace inertia::conformance
