#include <inertia/detail/props_resolver.hpp>
#include <inertia/headers.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <initializer_list>
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

// Dot-joins any number of non-empty segments.
std::string joinPath(std::initializer_list<std::string_view> parts) {
    std::string out;
    for (const auto part : parts) {
        if (part.empty()) {
            continue;
        }
        if (!out.empty()) {
            out += '.';
        }
        out += part;
    }
    return out;
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
        // A plain JSON object value is filtered by sub-path like an Object prop.
        const bool walkable = prop.kind() == Prop::Kind::Object ||
                              (prop.kind() == Prop::Kind::Value && prop.value().is_object());
        if (!bypass && !selected(path)) {
            // An unselected object may still hold always descendants, which bypass the filter.
            return walkable ? resolveObject(prop, path, false, false) : std::nullopt;
        }
        addMergeMetadata(prop, path);
        if (addOnceMetadata(prop, path)) {
            return std::nullopt; // already cached by the client
        }
        const bool deferredNow = prop.deferGroup() && !ctx_->partial;
        if (deferredNow) {
            addDeferred(prop, path);
            return std::nullopt;
        }
        switch (prop.kind()) {
        case Prop::Kind::Value:
            if (walkable && !bypass) {
                return resolveObject(prop, path, false, true);
            }
            return std::optional<Json>(std::in_place, prop.value());
        case Prop::Kind::Closure:
            return resolveClosure(prop, path);
        case Prop::Kind::Object:
            return resolveObject(prop, path, bypass, true);
        }
        return std::nullopt;
    }

    [[nodiscard]] Resolved finish() && { return std::move(out_); }
    Resolved &result() { return out_; }

  private:
    // Merge metadata is suppressed by X-Inertia-Reset; on partials it needs the path to equal or
    // descend from an only entry (ancestors do not count) and not be excluded.
    [[nodiscard]] bool mergeAllowed(std::string_view path) const {
        if (std::ranges::find(ctx_->reset, path) != ctx_->reset.end()) {
            return false;
        }
        if (!ctx_->partial) {
            return true;
        }
        const bool onlyMatch =
            ctx_->only.empty() || std::ranges::any_of(ctx_->only, [&](const std::string &entry) {
                return isSelfOrDescendant(path, entry);
            });
        const bool excluded = std::ranges::any_of(ctx_->except, [&](const std::string &entry) {
            return isSelfOrDescendant(path, entry);
        });
        return onlyMatch && !excluded;
    }

    // Classifies the prop's merge spec into the merge metadata lists.
    void addMergeMetadata(const Prop &prop, const std::string &path) {
        const auto &spec = prop.mergeSpec();
        if (!spec || !mergeAllowed(path)) {
            return;
        }
        auto &meta = out_.meta;
        if (spec->mode == MergeMode::Deep) {
            meta.deepMergeProps.push_back(path);
        } else if (spec->appendPaths.empty() && spec->prependPaths.empty()) {
            (spec->mode == MergeMode::Prepend ? meta.prependProps : meta.mergeProps)
                .push_back(path);
        }
        for (const auto &sub : spec->appendPaths) {
            meta.mergeProps.push_back(joinPath({path, sub}));
        }
        for (const auto &sub : spec->prependPaths) {
            meta.prependProps.push_back(joinPath({path, sub}));
        }
        for (const auto &match : spec->matchOn) {
            meta.matchPropsOn.push_back(joinPath({path, match.subPath, match.key}));
        }
    }

    // Records the onceProps entry. Returns true when the client already holds the prop: an
    // Inertia full visit, not fresh, whose key is in X-Inertia-Except-Once-Props.
    bool addOnceMetadata(const Prop &prop, const std::string &path) {
        const auto &spec = prop.onceSpec();
        if (!spec) {
            return false;
        }
        const std::string key = spec->key.value_or(path);
        Json expiresAt = nullptr;
        if (spec->ttl) {
            const auto expiry = ctx_->now + *spec->ttl;
            expiresAt =
                std::chrono::duration_cast<std::chrono::milliseconds>(expiry.time_since_epoch())
                    .count();
        }
        out_.meta.onceProps[key] = Json{{"prop", path}, {"expiresAt", std::move(expiresAt)}};
        return ctx_->inertia && !ctx_->partial && !spec->fresh &&
               std::ranges::find(ctx_->exceptOnce, key) != ctx_->exceptOnce.end();
    }

    void addDeferred(const Prop &prop, const std::string &path) {
        const auto &deferGroup = prop.deferGroup();
        if (!deferGroup) {
            return;
        }
        const auto &group = *deferGroup;
        auto &groups = out_.meta.deferredProps;
        auto it = std::ranges::find(groups, group, &Metadata::DeferredGroup::first);
        if (it == groups.end()) {
            it = groups.emplace(groups.end(), group, std::vector<std::string>{});
        }
        it->second.push_back(path);
    }

    // A deferred prop with rescue() that throws is omitted and reported in rescuedProps. Only
    // std::exception is rescued; anything else is not a resolver failure we can describe.
    std::optional<Json> resolveClosure(const Prop &prop, const std::string &path) {
        if (!prop.deferGroup() || !prop.rescues()) {
            return resolve(prop.resolver()(), path, true);
        }
        try {
            return resolve(prop.resolver()(), path, true);
        } catch (const std::exception &) {
            out_.meta.rescuedProps.push_back(path);
            return std::nullopt;
        }
    }

    std::optional<Json> resolveObject(const Prop &prop, const std::string &path, bool bypass,
                                      bool selectedSelf) {
        Json result = Json::object();
        const Props children = childrenForMerge(prop);
        for (const auto &[key, child] : children) {
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
        if (result.empty() && !bypass && (!selectedSelf || !children.empty())) {
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
