#pragma once

#include <inertia/json.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/props.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia::detail {

// Per-request resolution inputs, derived from request headers by makeContext().
struct ResolveContext {
    bool inertia = false;
    bool partial = false;
    std::vector<std::string> only, except, reset, exceptOnce;
    bool prependIntent = false;
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
};

// Page metadata collected while resolving; emitted by Inertia::page only when non-empty.
struct Metadata {
    std::vector<std::string> mergeProps, prependProps, deepMergeProps, matchPropsOn, rescuedProps;
    std::vector<std::pair<std::string, std::vector<std::string>>> deferredProps;
    Json scrollProps = Json::object(), onceProps = Json::object();
};

struct Resolved {
    Json props = Json::object();
    Metadata meta;
};

// Pure function of its arguments: all per-request state lives on the call stack.
[[nodiscard]] Resolved resolveProps(const Props &props, const ResolveContext &ctx);

// partial is true when the request is an Inertia request whose X-Inertia-Partial-Component
// equals `component`.
[[nodiscard]] ResolveContext makeContext(const RequestView &req, std::string_view component);

} // namespace inertia::detail
