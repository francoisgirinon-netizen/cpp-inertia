#include "routes.hpp"
#include "support.hpp"

#include <inertia/props.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace example {

namespace {

// Stands in for a slow query so the loading state is visible.
constexpr std::chrono::milliseconds kSlowQuery{300};

inertia::Json slowQuery(inertia::Json result) {
    std::this_thread::sleep_for(kSlowQuery);
    return result;
}

} // namespace

// Dashboard: deferred props in two groups (each group is fetched by its own follow-up request)
// and a once prop (resolved on the first visit, then remembered by the client).
void registerDashboard() {
    registerPage(
        {.path = "/dashboard", .component = "Dashboard"}, [](const ::drogon::HttpRequestPtr &) {
            static std::atomic<int> planResolutions{0};
            return inertia::Props{
                {"title", "Dashboard"},
                {"metrics",
                 inertia::defer(
                     [] { return inertia::Prop{slowQuery({{"visitors", 1280}, {"orders", 42}})}; },
                     "metrics")},
                {"activity", inertia::defer(
                                 [] {
                                     return inertia::Prop{slowQuery(inertia::Json::array(
                                         {"Ada signed in", "Alan placed an order"}))};
                                 },
                                 "activity")},
                {"plans", inertia::once([] {
                     return inertia::Prop{
                         inertia::Json{{"names", inertia::Json::array({"Free", "Pro"})},
                                       {"resolutions", ++planResolutions}}};
                 })},
            };
        });
}

} // namespace example
