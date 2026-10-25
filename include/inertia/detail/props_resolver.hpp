#pragma once

#include <inertia/core_export.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/props.hpp>

#include <chrono>
#include <cstddef>
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
    using DeferredGroup = std::pair<std::string, std::vector<std::string>>;
    std::vector<DeferredGroup> deferredProps;
    Json scrollProps = Json::object(), onceProps = Json::object();
};

struct Resolved {
    Json props = Json::object();
    Metadata meta;
};

// Exported (here and below) only so that the unit tests can reach it in a shared build: this header
// is not installed and the functions are not API.
// Pure function of its arguments: all per-request state lives on the call stack.
[[nodiscard]] INERTIA_CORE_EXPORT Resolved resolveProps(const Props &props,
                                                        const ResolveContext &ctx);

// Same result, but JSON leaf values and nested children are moved into the output, so `props` is
// left moved-from.
[[nodiscard]] INERTIA_CORE_EXPORT Resolved resolveProps(Props &&props, const ResolveContext &ctx);

// Reserves room for `count` members of a JSON object. ordered_json stores its members as
// pair<const std::string, Json>, which is not nothrow-movable, so a growing object copies every
// member value deeply on each reallocation; reserving up front keeps insertions as moves.
inline void reserveMembers(Json &object, std::size_t count) {
    object.get_ref<Json::object_t &>().reserve(count);
}

// partial is true when the request is an Inertia request whose X-Inertia-Partial-Component
// equals `component`.
[[nodiscard]] INERTIA_CORE_EXPORT ResolveContext makeContext(const RequestView &req,
                                                             std::string_view component);

} // namespace inertia::detail
