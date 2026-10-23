#include <inertia/detail/props_resolver.hpp>
#include <inertia/headers.hpp>
#include <inertia/inertia.hpp>

#include <algorithm>
#include <iterator>
#include <span>
#include <string>
#include <utility>

namespace inertia {

namespace {

// An Inertia request carries the X-Inertia header.
bool isInertia(const RequestView &req) { return req.header(headers::kInertia).has_value(); }

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
    : config_(std::move(config)), http_(std::move(http)) {}

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
    Props all = shared_;
    for (const auto &perRequest : perRequest_) {
        auto extra = perRequest(req, session);
        std::ranges::move(extra, std::back_inserter(all));
    }
    const auto sharedCount = all.size();
    all.insert(all.end(), props.begin(), props.end());

    auto ctx = detail::makeContext(req, component);
    if (config_.clock) {
        ctx.now = config_.clock();
    }
    auto resolved = detail::resolveProps(all, ctx);
    // errors stays the last key; a real always-prop in a later task.
    resolved.props.erase("errors");
    resolved.props["errors"] = Json::object();

    Json page = Json::object();
    page["component"] = std::string(component);
    page["props"] = std::move(resolved.props);
    page["url"] = req.url();
    page["version"] = version();
    if (config_.exposeSharedPropKeys && sharedCount > 0) {
        page["sharedProps"] = sharedKeys(std::span(all).first(sharedCount));
    }
    emitMetadata(page, resolved.meta);
    if (opts.encryptHistory.value_or(config_.encryptHistory)) {
        page["encryptHistory"] = true;
    }
    return page;
}

// By-value parameters are part of the public contract: they are consumed asynchronously by later
// tasks (deferred props).
// NOLINTBEGIN(performance-unnecessary-value-param)
void Inertia::render(const RequestView &req, SessionStore &session, std::string component,
                     Props props, std::function<void(Response)> done, RenderOptions opts) const {
    const Json page = this->page(req, session, component, props, opts);
    Response res;
    res.status = 200;
    if (isInertia(req)) {
        res.setHeader(std::string(headers::kInertia), "true");
        res.setHeader("Content-Type", "application/json");
        res.body = dumpJson(page);
    } else {
        res.setHeader("Content-Type", "text/html; charset=utf-8");
        res.body = config_.rootTemplate(RootContext{
            .page = page, .head = "", .body = inertiaBody(page, config_.rootId), .viteTags = ""});
    }
    done(std::move(res));
}
// NOLINTEND(performance-unnecessary-value-param)

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

void Inertia::finalize(const RequestView &req, Response &res) const {
    const auto method = req.method();
    const bool seeOther = method == "PUT" || method == "PATCH" || method == "DELETE";
    if (isInertia(req) && seeOther && res.status == 302) {
        res.status = 303;
    }
    auto items = headers::parseList(res.header(headers::kVary));
    const bool present = std::ranges::any_of(
        items, [](const auto &item) { return headers::iequals(item, headers::kInertia); });
    if (!present) {
        items.emplace_back(headers::kInertia);
    }
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
