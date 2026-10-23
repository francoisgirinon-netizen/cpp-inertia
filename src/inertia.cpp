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

} // namespace inertia
