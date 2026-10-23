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

    auto resolved = detail::resolveProps(all, detail::makeContext(req, component));
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
    if (req.header(headers::kInertia)) {
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

void Inertia::finalize(const RequestView & /*req*/, Response &res) const {
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
