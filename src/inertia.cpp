#include <inertia/headers.hpp>
#include <inertia/inertia.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace inertia {

Inertia::Inertia(Config config, std::shared_ptr<HttpClient> http)
    : config_(std::move(config)), http_(std::move(http)) {}

std::string Inertia::version() const { return config_.version ? config_.version() : std::string{}; }

Json Inertia::page(const RequestView &req, SessionStore & /*session*/, std::string_view component,
                   const Props &props, const RenderOptions &opts) const {
    Json propsJson = Json::object();
    for (const auto &[key, value] : props) {
        propsJson[key] = value;
    }
    propsJson["errors"] = Json::object();

    Json page = Json::object();
    page["component"] = std::string(component);
    page["props"] = std::move(propsJson);
    page["url"] = req.url();
    page["version"] = version();
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
