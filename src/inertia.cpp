#include <inertia/detail/props_resolver.hpp>
#include <inertia/detail/session_keys.hpp>
#include <inertia/headers.hpp>
#include <inertia/inertia.hpp>
#include <inertia/vite.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inertia {

namespace {

// An Inertia request carries the X-Inertia header.
bool isInertia(const RequestView &req) { return req.header(headers::kInertia).has_value(); }

// Consumes a session flag; only a stored boolean true counts as set.
bool pullFlag(SessionStore &session, std::string_view key) {
    const auto value = session.pull(key);
    return value && value->is_boolean() && value->get<bool>();
}

// Top-level keys of the shared props (a dot key contributes its first segment), then "errors",
// de-duplicated and in order.
Json sharedKeys(std::span<const std::pair<std::string, Prop>> shared) {
    std::vector<std::string> keys;
    const auto add = [&keys](std::string key) {
        if (std::ranges::find(keys, key) == keys.end()) {
            keys.push_back(std::move(key));
        }
    };
    for (const auto &entry : shared) {
        add(entry.first.substr(0, entry.first.find('.')));
    }
    add("errors");
    return keys;
}

// Shapes the session's {bag: {field: [messages]}} into the errors prop: each field keeps its first
// message unless all-errors mode is on. With an error-bag header the default bag is scoped under
// that name; without one a lone default bag is returned unscoped.
Json errorsProp(const Json &stored, bool allErrors, const std::optional<std::string> &bagHeader) {
    const auto shape = [allErrors](const Json &fields) {
        Json out = Json::object();
        for (const auto &[field, messages] : fields.items()) {
            if (allErrors) {
                out[field] = messages;
            } else if (messages.is_array() && !messages.empty()) {
                out[field] = messages.front();
            }
        }
        return out;
    };
    Json out = Json::object();
    if (!stored.is_object()) {
        return out;
    }
    const bool onlyDefault = stored.size() == 1 && stored.contains("default");
    if (onlyDefault && !bagHeader) {
        return shape(stored["default"]);
    }
    for (const auto &[bag, fields] : stored.items()) {
        const bool header = bag == "default" && bagHeader;
        out[header ? *bagHeader : bag] = shape(fields);
    }
    return out;
}

// Largest integer a JavaScript number holds exactly.
constexpr std::int64_t kMaxSafeInteger = 9007199254740991LL;

// Replaces, in place and recursively, integers beyond +/-(2^53 - 1) by {"$bigint": "<digits>"}.
void wrapBigIntegers(Json &value) {
    if (value.is_number_unsigned()) {
        if (value.get<std::uint64_t>() > static_cast<std::uint64_t>(kMaxSafeInteger)) {
            value = Json{{"$bigint", std::to_string(value.get<std::uint64_t>())}};
        }
    } else if (value.is_number_integer()) {
        const auto number = value.get<std::int64_t>();
        if (number > kMaxSafeInteger || number < -kMaxSafeInteger) {
            value = Json{{"$bigint", std::to_string(number)}};
        }
    } else if (value.is_structured()) {
        for (auto &child : value) {
            wrapBigIntegers(child);
        }
    }
}

// Writes the non-empty page metadata keys in protocol order.
void emitMetadata(Json &page, const detail::Metadata &meta) {
    const auto list = [&page](const char *key, const std::vector<std::string> &items) {
        if (!items.empty()) {
            page[key] = items;
        }
    };
    list("mergeProps", meta.mergeProps);
    list("prependProps", meta.prependProps);
    list("deepMergeProps", meta.deepMergeProps);
    list("matchPropsOn", meta.matchPropsOn);
    if (!meta.deferredProps.empty()) {
        Json groups = Json::object();
        for (const auto &[group, paths] : meta.deferredProps) {
            groups[group] = paths;
        }
        page["deferredProps"] = std::move(groups);
    }
    list("rescuedProps", meta.rescuedProps);
    if (!meta.scrollProps.empty()) {
        page["scrollProps"] = meta.scrollProps;
    }
    if (!meta.onceProps.empty()) {
        page["onceProps"] = meta.onceProps;
    }
}

} // namespace

Inertia::Inertia(Config config, std::shared_ptr<HttpClient> http)
    : config_(std::move(config)), ssr_(config_.ssr, std::move(http), config_.vite) {}

std::string Inertia::version() const { return config_.version ? config_.version() : std::string{}; }

void Inertia::share(std::string key, Prop value) {
    shared_.emplace_back(std::move(key), std::move(value));
}

void Inertia::shareWith(std::function<Props(const RequestView &, SessionStore &)> perRequest) {
    perRequest_.push_back(std::move(perRequest));
}

void Inertia::shareOnce(std::string key, Resolver fn) {
    share(std::move(key), once(std::move(fn)));
}

Json Inertia::page(const RequestView &req, SessionStore &session, std::string_view component,
                   const Props &props, const RenderOptions &opts) const {
    return buildPage(req, session, std::string(component), Props(props), opts);
}

Json Inertia::buildPage(const RequestView &req, SessionStore &session, std::string component,
                        Props props, const RenderOptions &opts) const {
    Props all = shared_;
    for (const auto &perRequest : perRequest_) {
        auto extra = perRequest(req, session);
        std::ranges::move(extra, std::back_inserter(all));
    }
    const auto sharedCount = all.size();
    std::ranges::move(props, std::back_inserter(all));

    auto ctx = detail::makeContext(req, component);
    if (config_.clock) {
        ctx.now = config_.clock();
    }
    // The keys are read before `all` is consumed below.
    Json sharedPropKeys;
    if (config_.exposeSharedPropKeys && sharedCount > 0) {
        sharedPropKeys = sharedKeys(std::span(all).first(sharedCount));
    }
    // `all` holds copies of the shared props, so resolving may consume it.
    auto resolved = detail::resolveProps(std::move(all), ctx);
    // errors stays the last key and wins over a user prop of the same name.
    resolved.props.erase("errors");
    auto bagHeader = req.header(headers::kErrorBag);
    if (bagHeader && bagHeader->empty()) {
        bagHeader.reset(); // an empty header names no bag
    }
    resolved.props["errors"] =
        errorsProp(session.pull(detail::session_keys::kErrors).value_or(Json::object()),
                   config_.withAllErrors, bagHeader);

    const bool bigIntegers = opts.preserveBigIntegers.value_or(config_.preserveBigIntegers);
    if (bigIntegers) {
        wrapBigIntegers(resolved.props);
    }

    Json page = Json::object();
    page["component"] = std::move(component);
    page["props"] = std::move(resolved.props);
    page["url"] = req.url();
    page["version"] = version();
    if (!sharedPropKeys.is_null()) {
        page["sharedProps"] = std::move(sharedPropKeys);
    }
    emitMetadata(page, resolved.meta);
    if (pullFlag(session, detail::session_keys::kClearHistory)) {
        page["clearHistory"] = true;
    }
    if (opts.encryptHistory.value_or(config_.encryptHistory)) {
        page["encryptHistory"] = true;
    }
    if (pullFlag(session, detail::session_keys::kPreserveFragment)) {
        page["preserveFragment"] = true;
    }
    if (bigIntegers) {
        page["preserveBigIntegers"] = true;
    }
    auto flashData = session.pull(detail::session_keys::kFlash);
    if (flashData && flashData->is_object() && !flashData->empty()) {
        if (bigIntegers) {
            wrapBigIntegers(*flashData);
        }
        page["flash"] = std::move(*flashData);
    }
    return page;
}

void Inertia::render(const RequestView &req, SessionStore &session, std::string component,
                     Props props, std::function<void(Response)> done, RenderOptions opts) const {
    const Json page = buildPage(req, session, std::move(component), std::move(props), opts);
    Response res;
    res.status = 200;
    if (isInertia(req)) {
        res.setHeader(std::string(headers::kInertia), "true");
        res.setHeader("Content-Type", "application/json");
        res.body = dumpJson(page);
        done(std::move(res));
        return;
    }
    res.setHeader("Content-Type", "text/html; charset=utf-8");
    std::string viteTags = config_.vite ? config_.vite->tags() : std::string{};
    // The callback may run after render() returns, so it owns a copy of the page.
    std::function<void(std::optional<SsrResult>)> finish =
        [rootId = config_.rootId, rootTemplate = config_.rootTemplate, owned = page,
         base = std::move(res), tags = std::move(viteTags),
         cb = std::move(done)](std::optional<SsrResult> ssr) {
            RootContext ctx{.page = owned, .head = "", .body = "", .viteTags = tags};
            if (ssr) {
                ctx.head = std::move(ssr->head);
                ctx.body = std::move(ssr->body);
            } else {
                ctx.body = inertiaBody(owned, rootId);
            }
            Response out = base;
            out.body = rootTemplate(ctx);
            cb(std::move(out));
        };
    const std::string path = req.url().substr(0, req.url().find_first_of("?#"));
    if (!ssr_.shouldRender(path)) {
        finish(std::nullopt);
        return;
    }
    ssr_.render(page, path, finish);
}

std::optional<Response> Inertia::handleVersion(const RequestView &req) const {
    if (!isInertia(req) || req.method() != "GET") {
        return std::nullopt;
    }
    const std::string current = version();
    if (req.header(headers::kVersion).value_or("") == current) {
        return std::nullopt;
    }
    Response res;
    res.status = 409;
    res.setHeader(std::string(headers::kLocation), req.fullUrl());
    res.setHeader(std::string(headers::kVersion), current);
    return res;
}

Response Inertia::location(const RequestView &req, std::string_view url) const {
    Response res;
    if (isInertia(req)) {
        res.status = 409;
        res.setHeader(std::string(headers::kLocation), std::string(url));
    } else {
        res.status = 302;
        res.setHeader("Location", std::string(url));
    }
    return res;
}

void Inertia::finalize(const RequestView &req, Response &res) const {
    // An empty 200 has nothing to show, so the client is sent back to where it came from.
    if (isInertia(req) && res.status == 200 && res.body.empty()) {
        res.status = 302;
        res.setHeader("Location", req.header("Referer").value_or("/"));
    }
    const auto method = req.method();
    const bool seeOther = method == "PUT" || method == "PATCH" || method == "DELETE";
    if (isInertia(req) && seeOther && res.status == 302) {
        res.status = 303;
    }
    const bool prefetch = headers::iequals(req.header(headers::kPurpose).value_or(""), "prefetch");
    const auto location = res.header("Location");
    if (isInertia(req) && !prefetch && res.status >= 300 && res.status < 400 && location &&
        location->find('#') != std::string::npos) {
        // Inertia cannot follow a fragment redirect, so the client is asked to visit it itself.
        std::erase_if(res.headers, [](const auto &entry) {
            return headers::iequals(entry.first, "Location") ||
                   headers::iequals(entry.first, headers::kInertia);
        });
        res.setHeader(std::string(headers::kRedirect), *location);
        res.status = 409;
        res.body.clear();
    }
    const auto existing = headers::parseList(res.header(headers::kVary));
    if (std::ranges::find(existing, "*") != existing.end()) {
        return; // Vary: * already covers every request header
    }
    std::vector<std::string> items;
    const auto add = [&items](std::string_view token) {
        if (!std::ranges::any_of(items,
                                 [&](const auto &item) { return headers::iequals(item, token); })) {
            items.emplace_back(token);
        }
    };
    for (const auto &token : existing) {
        add(token);
    }
    add(headers::kInertia);
    std::string joined;
    for (const auto &item : items) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += item;
    }
    res.setHeader(std::string(headers::kVary), std::move(joined));
}

} // namespace inertia
