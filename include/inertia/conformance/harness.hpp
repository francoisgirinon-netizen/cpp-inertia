#pragma once

#include <inertia/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// Conformance kit: HTTP-level protocol scenarios that an adapter runs against its own application.
///
/// An adapter provides send(), which performs one HTTP exchange, and builds the test executable
/// with inertia_add_conformance_test(<target> <harness sources>... [LINK <library>...]). The helper
/// links the scenarios and Catch2, and registers each scenario with CTest under the label
/// `conformance`.
namespace inertia::conformance {

/// One HTTP request, as the scenarios send it through send().
struct TestRequest {
    /// HTTP method, in uppercase.
    std::string method = "GET";
    /// Path and query string, such as "/users?page=2".
    std::string url = "/";
    /// Request headers, in the order they are sent.
    std::vector<std::pair<std::string, std::string>> headers;
    /// Request body; empty when the request has none.
    std::string body;
};

/// The response that an application produced, as a scenario observes it.
struct TestResponse {
    /// HTTP status code.
    int status = 0;
    /// Response headers, in the order received. A repeated header appears once per line.
    std::vector<std::pair<std::string, std::string>> headers;
    /// Response body.
    std::string body;

    /// Looks up a response header, case-insensitively.
    ///
    /// @param name Header name, in any case.
    /// @return The value of the first matching header, or std::nullopt when there is none.
    [[nodiscard]] std::optional<std::string> header(std::string_view name) const;

    /// Parses the body as JSON.
    ///
    /// @return The parsed document.
    /// @throws The JSON library's parse error when the body is not valid JSON.
    [[nodiscard]] Json json() const;
};

/// Performs one HTTP exchange with the application under test.
///
/// Each adapter's harness implements this function, so that the same scenarios run unchanged
/// against every adapter. The contract is as follows:
///  - a session persists across calls, as if one browser sent them all: keep a cookie jar over real
///    HTTP, or one session object in-process;
///  - a request to POST /__reset starts a fresh session (the scenarios send it before each test),
///    answers 204 and never reaches the application;
///  - the response is returned as the application produced it: redirects are not followed, and
///    every header is kept.
///
/// @param req The request to send.
/// @return The response the application answered.
TestResponse send(const TestRequest &req);

} // namespace inertia::conformance
