#include "context.hpp"
#include "routes.hpp"

#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace example {

namespace {

constexpr int kPageSize = 3;

// `edited` marks a post the server changed since the client last loaded it.
inertia::Json post(int id, const char *edited = "") {
    return {{"id", id}, {"title", "Post " + std::to_string(id) + edited}};
}

} // namespace

// Posts: merge props. `posts` appends each requested page, replacing entries with the same id
// (matchOn); `latest` prepends. `router.reload({ reset: ['posts'] })` starts over.
void registerPosts(Router &router) {
    registerPage(router, {.path = "/posts", .component = "Posts"}, [](Context &ctx) {
        const int page = std::max(intQuery(ctx, "page", 1), 1);
        // How many `latest` entries the client already shows. The new entry gets id newest + 100,
        // so each prepend adds the next id (100, 101, ...) as long as only this button adds them.
        const int newest = intQuery(ctx, "newest", 0);
        const bool edit = intQuery(ctx, "edit", 0) == 1; // re-send the last post, edited
        const int step = std::max(intQuery(ctx, "step", 1), 1);
        return inertia::Props{
            // With edit=1, page 2 and later re-send the last post of the page before, edited, so
            // the
            // matchOn reload carries an id the client already shows and must replace, not
            // duplicate.
            {"posts", inertia::Prop{[page, edit] {
                          auto posts = inertia::Json::array();
                          const int first = (page - 1) * kPageSize + 1;
                          if (page > 1 && edit) {
                              posts.push_back(post(first - 1, " (edited)"));
                          }
                          for (int id = first; id <= page * kPageSize; ++id) {
                              posts.push_back(post(id));
                          }
                          return posts;
                      }}.merge()
                          .matchOn("id")},
            {"page", page},
            // deepMerge(): objects are merged recursively on the client, so reloading adds this
            // step's tag next to the existing ones instead of replacing the whole object.
            {"settings", inertia::deepMerge(inertia::Prop{[step] {
                 return inertia::Json{
                     {"filters", {{"tags", inertia::Json::array({"tag" + std::to_string(step)})}}}};
             }})},
            // merge() with nested paths: `data` is appended and `meta` is prepended, each matched
            // by `id`, while the rest of the object is replaced.
            {"thread", inertia::merge(inertia::Prop{[step] {
                           return inertia::Json{
                               {"data", inertia::Json::array({post(step * 2 - 1), post(step * 2)})},
                               {"meta", inertia::Json::array({post(step + 100)})}};
                       }})
                           .append("data", "id")
                           .prepend("meta", "id")},
            {"latest", inertia::Prop{[newest] {
                           return inertia::Json::array({post(newest + 100)});
                       }}.prepend()
                           .matchOn("id")},
        };
    });
}

} // namespace example
