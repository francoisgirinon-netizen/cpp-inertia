#include "routes.hpp"
#include "support.hpp"

#include <inertia/props.hpp>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
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
            static std::atomic<int> eventCount{0};
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
                // defer().rescue(): when the resolver throws, the response still succeeds and lists
                // the prop under rescuedProps; the client then shows <Deferred rescue>.
                {"risky",
                 inertia::defer(
                     []() -> inertia::Prop { throw std::runtime_error("report is down"); }, "risky")
                     .rescue()},
                // defer().merge(): fetched after the first paint; every later reload of `events`
                // appends to the list instead of replacing it.
                {"events", inertia::defer(
                               [] {
                                   const int id = ++eventCount;
                                   return inertia::Prop{inertia::Json::array(
                                       {{{"id", id}, {"label", "Event " + std::to_string(id)}}})};
                               },
                               "events")
                               .merge()},
                {"plans", inertia::once([] {
                     return inertia::Prop{
                         inertia::Json{{"names", inertia::Json::array({"Free", "Pro"})},
                                       {"resolutions", ++planResolutions}}};
                 })},
            };
        });
}

} // namespace example
