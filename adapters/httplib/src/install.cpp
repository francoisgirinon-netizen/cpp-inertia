#include "random.hpp"

#include <inertia/httplib/httplib.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>
#include <inertia/response.hpp>

#include <httplib.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia::httplib {

namespace {

constexpr std::string_view kTokenKey = "inertia.csrf_token";
constexpr std::size_t kTokenBytes = 32;

// What the pre-routing handler learned about a request, handed to the route handler and to the
// post-routing handler through the response's user data (cpp-httplib's own per-request slot).
//
// The session is opened lazily (only a handler that asks for it, or a CSRF check, starts one), so
// requests that never touch it cost no memory and get no cookie.
struct SessionSlot {
    std::optional<std::string> incomingId; // from the cookie, not yet looked up
    bool opened = false;
    std::string id;
    std::shared_ptr<SessionData> data;
    bool announce = false; // the client must be told the id (new or rotated)
};

struct RequestState {
    std::shared_ptr<const Inertia> inertia;
    std::shared_ptr<const Options> options;
    std::shared_ptr<SessionSlot> slot;
    bool secure = false;
};

SessionSlot &ensureSession(const RequestState &state) {
    auto &slot = *state.slot;
    if (!slot.opened) {
        auto opened = state.options->sessions->open(
            slot.incomingId ? std::optional<std::string_view>{*slot.incomingId} : std::nullopt);
        slot.id = std::move(opened.id);
        slot.data = std::move(opened.data);
        slot.announce = opened.created;
        slot.opened = true;
    }
    return slot;
}

const std::string &stateKey() {
    static const std::string key = "inertia.httplib.state";
    return key;
}

const RequestState &requireState(const ::httplib::Response &res) {
    const auto *state = res.user_data.get<RequestState>(stateKey());
    if (state == nullptr) {
        throw std::logic_error(
            "inertia::httplib::install() has not been called for the server handling this request");
    }
    return *state;
}

void logError(const Options &options, std::string_view message) {
    if (options.logError) {
        options.logError(message);
        return;
    }
    std::cerr << "inertia: " << message << '\n';
}

Response plain(int status, std::string body) {
    Response res;
    res.status = status;
    res.headers.emplace_back("Content-Type", "text/plain");
    res.body = std::move(body);
    return res;
}

Response serverError() { return plain(500, "Internal Server Error"); }

// Laravel's "page expired".
Response pageExpired() { return plain(419, "CSRF token mismatch"); }

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

// The first cookie called `name` over every Cookie header line.
std::optional<std::string> requestCookie(const ::httplib::Request &req, std::string_view name) {
    const auto [first, last] = req.headers.equal_range("Cookie");
    for (auto line = first; line != last; ++line) {
        std::string_view rest = line->second;
        while (!rest.empty()) {
            const auto semicolon = rest.find(';');
            const auto item = trim(rest.substr(0, semicolon));
            rest = semicolon == std::string_view::npos ? std::string_view{}
                                                       : rest.substr(semicolon + 1);
            const auto eq = item.find('=');
            if (eq != std::string_view::npos && trim(item.substr(0, eq)) == name) {
                return std::string{trim(item.substr(eq + 1))};
            }
        }
    }
    return std::nullopt;
}

// Compares without exiting early on the first differing byte.
bool constantTimeEquals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned>(static_cast<unsigned char>(a[i])) ^
                static_cast<unsigned>(static_cast<unsigned char>(b[i]));
    }
    return diff == 0;
}

bool isSafeMethod(std::string_view method) {
    return method == "GET" || method == "HEAD" || method == "OPTIONS";
}

bool isExempt(const CsrfOptions &csrf, std::string_view path) {
    return std::ranges::any_of(csrf.exceptPaths, [&](const std::string &rule) {
        if (!rule.empty() && rule.back() == '*') {
            return path.starts_with(std::string_view{rule}.substr(0, rule.size() - 1));
        }
        return path == rule;
    });
}

// The session's CSRF token, created on first use; getOrCreate runs under the session lock, so two
// concurrent first requests of one session agree on a single token.
std::string tokenOf(HttplibSession &session) {
    const auto stored =
        session.getOrCreate(kTokenKey, [] { return Json(detail::randomHex(kTokenBytes)); });
    if (stored.is_string()) {
        return stored.get<std::string>();
    }
    auto token = detail::randomHex(kTokenBytes);
    session.put(std::string{kTokenKey}, Json(token));
    return token;
}

bool csrfMatches(const CsrfOptions &csrf, const ::httplib::Request &req,
                 const HttplibSession &session) {
    const auto sent = req.get_header_value(csrf.header);
    const auto expected = session.get(kTokenKey);
    return !sent.empty() && expected && expected->is_string() &&
           constantTimeEquals(sent, expected->get<std::string>());
}

void addSetCookie(::httplib::Response &res, std::string line) {
    res.headers.emplace("Set-Cookie", std::move(line));
}

// Sets the token cookie unless the client already presents the current token.
void attachCsrfCookie(const CsrfOptions &csrf, const ::httplib::Request &req,
                      const RequestState &state, ::httplib::Response &res) {
    HttplibSession session{state.slot->data};
    const auto token = tokenOf(session);
    if (constantTimeEquals(requestCookie(req, csrf.cookie).value_or(""), token)) {
        return;
    }
    // Scripts must read it to echo it back in the header, so it is not HttpOnly.
    std::string line = csrf.cookie + "=" + token + "; Path=/; SameSite=Lax";
    if (state.secure) {
        line += "; Secure";
    }
    addSetCookie(res, std::move(line));
}

using HeaderGroups = std::map<std::string, std::vector<std::string>>;

std::string lowered(std::string_view name) {
    std::string out{name};
    std::ranges::transform(out, out.begin(), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return out;
}

HeaderGroups groupByName(const Response &res) {
    HeaderGroups groups;
    for (const auto &[name, value] : res.headers) {
        groups[lowered(name)].push_back(value);
    }
    return groups;
}

// Runs Inertia::finalize on the response and writes its edits (status, body, headers) back.
void finalizeResponse(const Inertia &inertia, const HttplibRequest &view,
                      ::httplib::Response &res) {
    Response edited = fromHttplib(res);
    const Response before = edited;
    inertia.finalize(view, edited);

    if (edited.status != before.status) {
        res.status = edited.status;
    }
    if (edited.body != before.body) {
        res.body = edited.body;
    }
    const auto old = groupByName(before);
    const auto now = groupByName(edited);
    std::set<std::string> changed;
    for (const auto &[name, values] : old) {
        const auto it = now.find(name);
        if (it == now.end()) {
            res.headers.erase(name); // case-insensitive: drops every line of that name
        } else if (it->second != values) {
            changed.insert(name);
        }
    }
    for (const auto &[name, values] : now) {
        if (!old.contains(name)) {
            changed.insert(name);
        }
    }
    for (const auto &name : changed) {
        res.headers.erase(name);
    }
    for (const auto &[name, value] : edited.headers) {
        if (changed.contains(lowered(name))) {
            res.headers.emplace(name, value);
        }
    }
}

// Tracks which servers carry an installation, by address. The entry is tied to a token that only
// the server's own handlers hold: when the server is destroyed the token expires, so a later
// server allocated at the same address is not mistaken for the old one.
std::shared_ptr<void> claim(const ::httplib::Server &server) {
    static std::mutex mutex;
    static std::map<const ::httplib::Server *, std::weak_ptr<void>> installed;
    const std::scoped_lock lock{mutex};
    std::erase_if(installed, [](const auto &entry) { return entry.second.expired(); });
    if (installed.contains(&server)) {
        throw std::logic_error("inertia::httplib::install() was already called for this server");
    }
    auto token = std::make_shared<char>();
    installed.emplace(&server, token);
    return token;
}

} // namespace

void install(::httplib::Server &server, const std::shared_ptr<const Inertia> &inertia,
             Options options) {
    if (!inertia) {
        throw std::invalid_argument("inertia::httplib::install() needs an Inertia instance");
    }
    const auto token = claim(server);
    if (!options.sessions) {
        options.sessions = std::make_shared<MemorySessionStore>();
    }
    const auto shared = std::make_shared<const Options>(std::move(options));

    server.set_pre_routing_handler(
        [inertia, shared, token](const ::httplib::Request &req, ::httplib::Response &res) {
            try {
                const HttplibRequest view{req, shared->trustForwardedHeaders};
                RequestState state{.inertia = inertia,
                                   .options = shared,
                                   .slot = std::make_shared<SessionSlot>(),
                                   .secure = view.isSecure()};
                state.slot->incomingId = requestCookie(req, shared->sessions->cookieName());

                std::optional<Response> early = inertia->handleVersion(view);
                if (!early && shared->csrf.enabled && !isSafeMethod(view.method()) &&
                    !isExempt(shared->csrf, req.path) &&
                    !csrfMatches(shared->csrf, req, HttplibSession{ensureSession(state).data})) {
                    early = pageExpired(); // the session exists now, so the answer carries a token
                }
                res.user_data.set(stateKey(), std::move(state));
                if (early) {
                    // cpp-httplib still runs the post-routing handler for this response.
                    toHttplib(*early, res);
                    return ::httplib::Server::HandlerResponse::Handled;
                }
                if (shared->preRouting) {
                    return shared->preRouting(req, res);
                }
                return ::httplib::Server::HandlerResponse::Unhandled;
            } catch (const std::exception &error) {
                logError(*shared, std::string{"pre-routing failed: "} + error.what());
                toHttplib(serverError(), res);
            }
            return ::httplib::Server::HandlerResponse::Handled;
        });

    server.set_post_routing_handler(
        [shared, token](const ::httplib::Request &req, ::httplib::Response &res) {
            try {
                if (shared->postRouting) {
                    shared->postRouting(req, res); // before Inertia's step: finalize stays last
                }
                const auto *state = res.user_data.get<RequestState>(stateKey());
                if (state == nullptr) {
                    return; // rejected before routing (malformed request): nothing to add
                }
                const auto &slot = *state->slot;
                if (slot.opened) {
                    if (slot.announce) {
                        addSetCookie(res, shared->sessions->cookieHeader(slot.id, state->secure));
                    }
                    if (shared->csrf.enabled) {
                        attachCsrfCookie(shared->csrf, req, *state, res);
                    }
                }
                finalizeResponse(*state->inertia,
                                 HttplibRequest{req, shared->trustForwardedHeaders}, res);
            } catch (const std::exception &error) {
                logError(*shared, std::string{"post-routing failed: "} + error.what());
            }
        });
}

void render(const ::httplib::Request &req, ::httplib::Response &res, std::string component,
            Props props, RenderOptions opts) {
    const auto &state = requireState(res);

    // The callback may fire from another thread (a custom asynchronous HttpClient for SSR); this
    // function waits for it, so the request and session views below outlive the render.
    struct Pending {
        std::promise<Response> promise;
        std::atomic<bool> answered{false};
        void answer(Response response) {
            if (!answered.exchange(true)) {
                promise.set_value(std::move(response));
            }
        }
    };
    const auto pending = std::make_shared<Pending>();
    auto future = pending->promise.get_future();

    Response answer;
    try {
        const HttplibRequest view{req, state.options->trustForwardedHeaders};
        HttplibSession session{ensureSession(state).data};
        state.inertia->render(
            view, session, std::move(component), std::move(props),
            [pending](Response response) { pending->answer(std::move(response)); }, opts);
        answer = future.get();
    } catch (const std::exception &error) {
        logError(*state.options, std::string{"render failed: "} + error.what());
        answer = serverError();
    } catch (...) {
        logError(*state.options, "render failed with a non-standard exception");
        answer = serverError();
    }
    toHttplib(answer, res);
}

void location(const ::httplib::Request &req, ::httplib::Response &res, std::string_view url) {
    const auto &state = requireState(res);
    toHttplib(
        state.inertia->location(HttplibRequest{req, state.options->trustForwardedHeaders}, url),
        res);
}

HttplibSession session(const ::httplib::Response &res) {
    return HttplibSession{ensureSession(requireState(res)).data};
}

void regenerateSession(const ::httplib::Response &res) {
    const auto &state = requireState(res);
    auto &slot = ensureSession(state);
    auto rotated = state.options->sessions->rotate(slot.id);
    slot.id = std::move(rotated.id);
    slot.data = std::move(rotated.data);
    slot.announce = true;
    (void)HttplibSession{slot.data}.pull(kTokenKey); // a new session gets a new CSRF token
}

} // namespace inertia::httplib
