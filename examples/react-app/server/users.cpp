#include "routes.hpp"
#include "support.hpp"

#include <inertia/drogon/drogon.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>
#include <inertia/session.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpTypes.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace example {

namespace {

struct User {
    int id;
    std::string name;
    std::string email;
    bool admin = false;
};

// In-memory "database".
class UserStore {
  public:
    [[nodiscard]] inertia::Json search(const std::string &term) const {
        const std::scoped_lock lock{mutex_};
        auto matches = inertia::Json::array();
        for (const auto &user : users_) {
            if (contains(user.name, term) || contains(user.email, term)) {
                matches.push_back({{"id", user.id},
                                   {"name", user.name},
                                   {"email", user.email},
                                   {"admin", user.admin}});
            }
        }
        return matches;
    }

    void add(std::string name, std::string email) {
        const std::scoped_lock lock{mutex_};
        users_.push_back({.id = nextId_++, .name = std::move(name), .email = std::move(email)});
    }

    void promote(int id) {
        const std::scoped_lock lock{mutex_};
        for (auto &user : users_) {
            if (user.id == id) {
                user.admin = true;
            }
        }
    }

  private:
    static bool contains(const std::string &haystack, const std::string &needle) {
        const auto lower = [](std::string text) {
            std::ranges::transform(text, text.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
            return text;
        };
        return lower(haystack).find(lower(needle)) != std::string::npos;
    }

    mutable std::mutex mutex_;
    std::vector<User> users_{{.id = 1, .name = "Ada Lovelace", .email = "ada@example.com"},
                             {.id = 2, .name = "Alan Turing", .email = "alan@example.com"},
                             {.id = 3, .name = "Grace Hopper", .email = "grace@example.com"}};
    int nextId_ = 4;
};

UserStore &store() {
    static UserStore instance;
    return instance;
}

std::string stringField(const inertia::Json &body, const char *key) {
    const auto found = body.find(key);
    return found != body.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

} // namespace

// Users: a partial reload driven by a search filter, a form with validation errors and a flash
// message, and a PUT whose 302 redirect Inertia upgrades to a 303.
void registerUsers() {
    // GET /users?search=... The client reloads with only: ['users'], so `renderCount` (resolved
    // lazily on every render that includes it) and `filters` stay untouched on a partial reload.
    registerPage({.path = "/users", .component = "Users/Index"},
                 [](const ::drogon::HttpRequestPtr &req) {
                     const auto search = req->getParameter("search");
                     static std::atomic<int> renders{0};
                     return inertia::Props{
                         {"users", [search] { return inertia::Prop{store().search(search)}; }},
                         {"filters", inertia::Json{{"search", search}}},
                         {"renderCount", [] { return inertia::Prop{++renders}; }},
                     };
                 });

    registerPage({.path = "/users/create", .component = "Users/Create"},
                 [](const ::drogon::HttpRequestPtr &) { return inertia::Props{}; });

    // POST /users: invalid input goes back to the form with errors (consumed by the next render);
    // valid input is saved, flashed and redirected.
    ::drogon::app().registerHandler(
        "/users",
        [](const ::drogon::HttpRequestPtr &req, Callback &&done) {
            const auto body = jsonBody(req);
            const auto name = stringField(body, "name");
            const auto email = stringField(body, "email");

            inertia::Errors errors;
            if (name.empty()) {
                errors.emplace_back("name", std::vector<std::string>{"The name is required."});
            }
            if (email.find('@') == std::string::npos) {
                errors.emplace_back("email", std::vector<std::string>{"The email must contain @."});
            }
            auto session = sessionOf(req);
            if (!errors.empty()) {
                inertia::withErrors(session, errors);
                return reply(std::move(done), inertia::back(inertia::drogon::DrogonRequest{req}));
            }
            store().add(name, email);
            inertia::flash(session, "success", "User created.");
            reply(std::move(done), inertia::redirect("/users"));
        },
        {::drogon::Post});

    // PUT /users/{id}/promote answers 302; Inertia turns it into 303 so the browser follows with
    // GET.
    ::drogon::app().registerHandler(
        "/users/{id}/promote",
        [](const ::drogon::HttpRequestPtr &req, Callback &&done, int id) {
            store().promote(id);
            auto session = sessionOf(req);
            inertia::flash(session, "success", "User promoted.");
            reply(std::move(done), inertia::redirect("/users"));
        },
        {::drogon::Put});
}

} // namespace example
