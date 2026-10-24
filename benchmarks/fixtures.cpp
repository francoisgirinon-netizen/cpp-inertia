#include "fixtures.hpp"

#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/ports/request_view.hpp>
#include <inertia/ports/session_store.hpp>
#include <inertia/props.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace inertia::bench {

namespace {

// A text of 1 KB with characters that script escaping has to rewrite.
std::string text(std::size_t seed) {
    constexpr std::size_t kSize = 1024;
    static constexpr std::array<std::string_view, 5> fragments{
        "Lorem ipsum dolor sit amet, ",
        "see </script><b>bold</b> or a/b/c paths, ",
        "consectetur & adipiscing \"quoted\" elit; ",
        "caf\xC3\xA9 na\xC3\xAFve \xE2\x80\x94 dash, ",
        "x < y > z and http://example.com/a/b?c=d, ",
    };
    std::string out;
    while (out.size() < kSize) {
        out += fragments[seed++ % 5];
    }
    out.resize(kSize);
    return out;
}

} // namespace

Json user(int index) {
    const auto id = std::to_string(index);
    return Json{
        {"id", index},
        {"name", "User Number " + id},
        {"email", "user" + id + "@example.com"},
        {"role", index % 3 != 0 ? "member" : "admin"},
        {"active", index % 2 == 0},
        {"created_at", "2025-01-02T03:04:05Z"},
        {"avatar", "https://cdn.example.com/avatars/" + id + ".png"},
        {"meta", {{"logins", index * 7}, {"score", index * 1.5}, {"tags", {"a", "b", "c"}}}}};
}

Props makeProps(int largeItems) {
    Json users = Json::array();
    for (int i = 0; i < 50; ++i) {
        users.push_back(user(i));
    }
    Props p;
    p.emplace_back("user", user(1));
    p.emplace_back("users", std::move(users));
    p.emplace_back("stats", Json{{"posts", 123},
                                 {"comments", 4567},
                                 {"visits", 89012},
                                 {"ratio", 0.731},
                                 {"series", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}}});
    p.emplace_back("settings",
                   Json{{"theme", "dark"},
                        {"locale", "fr"},
                        {"notifications", {{"email", true}, {"push", false}, {"digest", "weekly"}}},
                        {"limits", {{"upload", 1048576}, {"rate", 60}}}});
    p.emplace_back("bio", text(0));
    p.emplace_back("notes", text(1));
    p.emplace_back("description", text(2));
    p.emplace_back("tags", Json::array({"c++", "inertia", "drogon", "web", "perf", "json", "html",
                                        "ssr", "vite", "react"}));
    p.emplace_back("flags", Json{{"beta", true}, {"admin", false}, {"trial", false}});
    p.emplace_back("pagination", Json{{"page", 1},
                                      {"per_page", 50},
                                      {"total", 1234},
                                      {"next", "/users?page=2"},
                                      {"prev", nullptr}});
    p.emplace_back("breadcrumbs", Json::array({Json{{"label", "Home"}, {"href", "/"}},
                                               Json{{"label", "Users"}, {"href", "/users"}}}));
    p.emplace_back("title", "Users");
    p.emplace_back("subtitle", "All the people");
    p.emplace_back("count", 50);
    p.emplace_back("ratio", 0.5);
    p.emplace_back("enabled", true);
    p.emplace_back("menu", Json::array({Json{{"label", "Dash"}, {"href", "/dash"}},
                                        Json{{"label", "Users"}, {"href", "/users"}},
                                        Json{{"label", "Posts"}, {"href", "/posts"}}}));
    p.emplace_back("lazy", Prop([] { return Prop(Json{{"computed", 42}, {"at", "now"}}); }));
    p.emplace_back("opt", optional([] { return Prop(Json("only on partial")); }));
    p.emplace_back("slow", defer([] { return Prop(Json::array({1, 2, 3})); }));
    p.emplace_back("feed", merge(Prop(Json::array({1, 2, 3}))));
    if (largeItems > 0) {
        Json items = Json::array();
        for (int i = 0; i < largeItems; ++i) {
            const auto id = std::to_string(i);
            items.push_back(Json{{"id", i},
                                 {"title", "Post title " + id},
                                 {"slug", "post-title-" + id},
                                 {"url", "/posts/" + id},
                                 {"excerpt", "Short excerpt with <em>html</em> and a/b slash"},
                                 {"views", i * 3},
                                 {"author", {{"id", i % 10}, {"name", "Author"}}}});
        }
        p.emplace_back("items", std::move(items));
    }
    return p;
}

void shareTypical(Inertia &inertia) {
    inertia.share("appName", "cpp-inertia demo");
    inertia.share("locale", "fr");
    inertia.share("auth", Json{{"user", user(1)}, {"permissions", {"a", "b", "c", "d"}}});
    Json routes = Json::object();
    for (int i = 0; i < 30; ++i) {
        routes["route" + std::to_string(i)] = "/path/" + std::to_string(i) + "/{id}";
    }
    inertia.share("routes", std::move(routes));
    inertia.share("csrf", "0123456789abcdef0123456789abcdef");
    inertia.shareWith([](const RequestView &, SessionStore &) {
        Props p;
        p.emplace_back("flashMsg", Json("hello"));
        return p;
    });
}

std::string manifestText(int entries) {
    Json manifest = Json::object();
    manifest["src/app.tsx"] = {{"file", "assets/app-AbCd1234.js"},
                               {"name", "app"},
                               {"src", "src/app.tsx"},
                               {"isEntry", true},
                               {"imports", {"_vendor-xyz.js", "_ui-abc.js"}},
                               {"css", {"assets/app-1234.css"}}};
    manifest["_vendor-xyz.js"] = {
        {"file", "assets/vendor-xyz.js"}, {"name", "vendor"}, {"css", {"assets/vendor.css"}}};
    manifest["_ui-abc.js"] = {
        {"file", "assets/ui-abc.js"}, {"name", "ui"}, {"imports", {"_vendor-xyz.js"}}};
    for (int i = 0; i < entries - 3; ++i) {
        const auto id = std::to_string(i);
        const auto key = "src/pages/Page" + id + ".tsx";
        manifest[key] = {{"file", "assets/Page" + id + "-Hash" + std::to_string(i * 7919) + ".js"},
                         {"name", "Page" + id},
                         {"src", key},
                         {"isDynamicEntry", true},
                         {"imports", {"src/app.tsx", "_vendor-xyz.js"}},
                         {"css", {"assets/Page" + id + ".css"}}};
    }
    return manifest.dump(2);
}

} // namespace inertia::bench
