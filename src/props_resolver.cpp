#include <inertia/detail/props_resolver.hpp>
#include <inertia/headers.hpp>

#include <algorithm>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace inertia::detail {

namespace {

std::vector<std::string> splitPath(std::string_view key) {
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (true) {
        const auto dot = key.find('.', start);
        if (dot == std::string_view::npos) {
            segments.emplace_back(key.substr(start));
            return segments;
        }
        segments.emplace_back(key.substr(start, dot - start));
        start = dot + 1;
    }
}

// True when `path` equals `base` or is a descendant of it.
bool isSelfOrDescendant(std::string_view path, std::string_view base) {
    return path == base ||
           (path.size() > base.size() && path.starts_with(base) && path[base.size()] == '.');
}

// The children of an existing prop when a dot key is merged into it.
Props childrenForMerge(const Prop &existing) {
    if (existing.kind() == Prop::Kind::Object) {
        return existing.children();
    }
    Props children;
    if (existing.kind() == Prop::Kind::Value && existing.value().is_object()) {
        for (const auto &[key, value] : existing.value().items()) {
            children.emplace_back(key, Prop(Json(value)));
        }
    }
    return children;
}

Prop mergeInto(const Prop &existing, std::span<const std::string> segments, Prop value);

// Inserts `value` at the dot-path `segments`, creating or extending nested objects. A plain key
// replaces an existing entry in place; a dot key merges into the existing nested prop.
void insertPath(Props &out, std::span<const std::string> segments, Prop value) {
    const auto &head = segments.front();
    auto it = std::ranges::find(out, head, &Props::value_type::first);
    if (segments.size() == 1) {
        if (it == out.end()) {
            out.emplace_back(head, std::move(value));
        } else {
            it->second = std::move(value);
        }
        return;
    }
    if (it == out.end()) {
        it = out.emplace(out.end(), head, object({}));
    }
    it->second = mergeInto(it->second, segments.subspan(1), std::move(value));
}

// Inserts `value` below `existing`, which keeps its category and modifiers. A closure stays lazy:
// the original runs first, then the child is inserted into its result.
Prop mergeInto(const Prop &existing, std::span<const std::string> segments, Prop value) {
    if (existing.kind() == Prop::Kind::Closure) {
        return existing.withResolver(
            [original = existing.resolver(),
             rest = std::vector<std::string>(segments.begin(), segments.end()),
             child = std::move(value)]() -> Prop { return mergeInto(original(), rest, child); });
    }
    Props children = childrenForMerge(existing);
    insertPath(children, segments, std::move(value));
    return existing.withChildren(std::move(children));
}

class Walker {
  public:
    explicit Walker(const ResolveContext &ctx) : ctx_(&ctx) {}

    // Resolves one prop. Returns nullopt when it is omitted from the response.
    // `bypass` is true below an always prop and below a resolved closure: their children skip
    // the only/except filter. Per-prop classification (defer/merge/once/scroll metadata) belongs
    // here, keyed by `path`.
    std::optional<Json> resolve(const Prop &prop, const std::string &path, bool bypass) {
        bypass = bypass || prop.isAlways();
        if (prop.isOptional() && !ctx_->partial) {
            return std::nullopt;
        }
        if (!bypass && !selected(path)) {
            // An unselected object may still hold always descendants, which bypass the filter.
            return prop.kind() == Prop::Kind::Object ? resolveObject(prop, path, false, false)
                                                     : std::nullopt;
        }
        switch (prop.kind()) {
        case Prop::Kind::Value:
            return std::optional<Json>(std::in_place, prop.value());
        case Prop::Kind::Closure:
            return resolve(prop.resolver()(), path, true);
        case Prop::Kind::Object:
            return resolveObject(prop, path, bypass, true);
        }
        return std::nullopt;
    }

    [[nodiscard]] Resolved finish() && { return std::move(out_); }
    Resolved &result() { return out_; }

  private:
    std::optional<Json> resolveObject(const Prop &prop, const std::string &path, bool bypass,
                                      bool selectedSelf) {
        Json result = Json::object();
        for (const auto &[key, child] : prop.children()) {
            std::string childPath = path;
            if (!childPath.empty()) {
                childPath += '.';
            }
            childPath += key;
            auto value = resolve(child, childPath, bypass);
            if (value) {
                result[key] = std::move(*value);
            }
        }
        if (result.empty() && !bypass && (!selectedSelf || !prop.children().empty())) {
            return std::nullopt; // everything below was filtered out
        }
        return std::optional<Json>(std::in_place, std::move(result));
    }

    [[nodiscard]] bool selected(std::string_view path) const {
        if (!ctx_->partial) {
            return true;
        }
        const bool onlyMatch =
            ctx_->only.empty() || std::ranges::any_of(ctx_->only, [&](const std::string &entry) {
                return isSelfOrDescendant(path, entry) || isSelfOrDescendant(entry, path);
            });
        const bool excluded = std::ranges::any_of(ctx_->except, [&](const std::string &entry) {
            return isSelfOrDescendant(path, entry);
        });
        return onlyMatch && !excluded;
    }

    const ResolveContext *ctx_;
    Resolved out_;
};

} // namespace

Resolved resolveProps(const Props &props, const ResolveContext &ctx) {
    Props normalized;
    for (const auto &[key, prop] : props) {
        insertPath(normalized, splitPath(key), prop);
    }
    Walker walker(ctx);
    for (const auto &[key, prop] : normalized) {
        auto value = walker.resolve(prop, key, false);
        if (value) {
            walker.result().props[key] = std::move(*value);
        }
    }
    return std::move(walker).finish();
}

ResolveContext makeContext(const RequestView &req, std::string_view component) {
    ResolveContext ctx;
    ctx.inertia = req.header(headers::kInertia).has_value();
    ctx.partial = ctx.inertia && req.header(headers::kPartialComponent) == component;
    ctx.only = headers::parseList(req.header(headers::kPartialData));
    ctx.except = headers::parseList(req.header(headers::kPartialExcept));
    ctx.reset = headers::parseList(req.header(headers::kReset));
    ctx.exceptOnce = headers::parseList(req.header(headers::kExceptOnce));
    ctx.prependIntent = req.header(headers::kMergeIntent) == "prepend";
    return ctx;
}

} // namespace inertia::detail
