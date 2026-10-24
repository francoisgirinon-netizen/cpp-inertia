#include "routes.hpp"
#include "support.hpp"

#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <drogon/HttpRequest.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <utility>

namespace example {

namespace {

constexpr int kTotalItems = 60;
constexpr int kPageSize = 10;
constexpr int kLastPage = (kTotalItems + kPageSize - 1) / kPageSize;

int pageNumber(const std::string &text) {
    int value = 1;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return std::clamp(value, 1, kLastPage);
}

} // namespace

// Feed: a scroll prop. The server returns one page of items plus the paging metadata; the
// client's <InfiniteScroll> asks for the next page and merges it into the list.
void registerFeed() {
    registerPage({.path = "/feed", .component = "Feed"}, [](const ::drogon::HttpRequestPtr &req) {
        const int current = pageNumber(req->getParameter("page"));
        return inertia::Props{
            {"items",
             inertia::scroll(
                 [current] {
                     auto items = inertia::Json::array();
                     for (int n = (current - 1) * kPageSize + 1; n <= current * kPageSize; ++n) {
                         items.push_back({{"id", n}, {"title", "Item " + std::to_string(n)}});
                     }
                     return inertia::Json{{"data", items}};
                 },
                 [current] {
                     inertia::ScrollMetadata paging{.pageName = "page"};
                     paging.currentPage = current;
                     if (current > 1) {
                         paging.previousPage = current - 1;
                     }
                     if (current < kLastPage) {
                         paging.nextPage = current + 1;
                     }
                     return paging;
                 })},
        };
    });
}

} // namespace example
