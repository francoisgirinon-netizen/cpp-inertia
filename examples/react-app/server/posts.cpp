#include "routes.hpp"
#include "support.hpp"

#include <inertia/props.hpp>

#include <algorithm>
#include <charconv>
#include <string>

namespace example {

namespace {

constexpr int kPageSize = 3;

int intParam(const ::drogon::HttpRequestPtr &req, const char *name, int fallback) {
    const auto text = req->getParameter(name);
    int value = fallback;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

inertia::Json post(int id) { return {{"id", id}, {"title", "Post " + std::to_string(id)}}; }

} // namespace

// Posts: merge props. `posts` appends each requested page, replacing entries with the same id
// (matchOn); `latest` prepends. `router.reload({ reset: ['posts'] })` starts over.
void registerPosts() {
    registerPage({.path = "/posts", .component = "Posts"}, [](const ::drogon::HttpRequestPtr &req) {
        const int page = std::max(intParam(req, "page", 1), 1);
        const int newest = intParam(req, "newest", 0); // the highest id the client has seen
        return inertia::Props{
            {"posts", inertia::Prop{[page] {
                          auto posts = inertia::Json::array();
                          for (int id = (page - 1) * kPageSize + 1; id <= page * kPageSize; ++id) {
                              posts.push_back(post(id));
                          }
                          return posts;
                      }}.merge()
                          .matchOn("id")},
            {"page", page},
            {"latest", inertia::Prop{[newest] {
                           return inertia::Json::array({post(newest + 100)});
                       }}.prepend()
                           .matchOn("id")},
        };
    });
}

} // namespace example
