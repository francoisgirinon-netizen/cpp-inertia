#include "context.hpp"
#include "routes.hpp"

#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>

namespace example {

// Variants: small props that exercise protocol corners the other pages do not. The page dumps its
// props as JSON, so a visit or reload shows exactly what the server sent.
void registerVariants(Router &router) {
    static std::atomic<int> resolvedBag{0};
    static std::atomic<int> freshResolved{0};
    static std::atomic<int> customResolved{0};
    static std::atomic<int> plainResolved{0};
    static std::atomic<int> lazyOnceResolved{0};

    router.get("/variants", [](Context &) -> Outcome {
        inertia::RenderOptions options;
        options.preserveBigIntegers = true; // integers beyond 2^53 become {"$bigint": "..."}
        inertia::Props props{
            // Top-level dot keys are unpacked into one nested `auth` object.
            {"auth.name", "Ada"},
            {"auth.role", "admin"},
            // The closure's result is not filtered again: `only: ['bag.x']` still gets `y`.
            {"bag", [] { return inertia::Prop{inertia::Json{{"x", 1}, {"y", ++resolvedBag}}}; }},
            // once props: `plain` is remembered by the client, `fresh` is always resolved
            // again, and `custom` is remembered under the key "shared-plan".
            {"plain", inertia::once([] {
                 return inertia::Prop{"plain #" + std::to_string(++plainResolved)};
             })},
            {"fresh", inertia::once([] {
                          return inertia::Prop{"fresh #" + std::to_string(++freshResolved)};
                      }).fresh()},
            {"custom", inertia::once([] {
                           return inertia::Prop{"custom #" + std::to_string(++customResolved)};
                       }).as("shared-plan")},
            // A deferred once prop: after the client has it, its key is sent back in
            // X-Inertia-Except-Once-Props and it is neither deferred nor resolved again.
            {"lazyOnce",
             inertia::defer(
                 [] { return inertia::Prop{"lazy once #" + std::to_string(++lazyOnceResolved)}; },
                 "once")
                 .once()},
            {"big", inertia::Json{{"id", std::int64_t{9007199254740993}}}},
            // A deferred scroll prop: no scrollProps until the follow-up request.
            {"lazyItems",
             inertia::scroll(
                 [] { return inertia::Json{{"data", inertia::Json::array({{{"id", 1}}})}}; },
                 [] {
                     inertia::ScrollMetadata paging;
                     paging.currentPage = 1;
                     return paging;
                 })
                 .deferred("lazy")},
        };
        return Page{.component = "Variants", .props = std::move(props), .options = options};
    });
}

} // namespace example
