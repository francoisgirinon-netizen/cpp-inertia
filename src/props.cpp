#include <inertia/props.hpp>

#include <utility>

namespace inertia {

Prop::Prop(Json v) : value_(std::move(v)) {}

MergeSpec &Prop::ensureMerge() {
    if (!mergeSpec_) {
        mergeSpec_.emplace();
    }
    return *mergeSpec_;
}

OnceSpec &Prop::ensureOnce() {
    if (!onceSpec_) {
        onceSpec_.emplace();
    }
    return *onceSpec_;
}

Prop Prop::prepend() && {
    ensureMerge().mode = MergeMode::Prepend;
    return std::move(*this);
}

Prop Prop::append(std::string subPath, std::optional<std::string> matchOn) && {
    auto &spec = ensureMerge();
    if (matchOn) {
        spec.matchOn.push_back({.subPath = subPath, .key = std::move(*matchOn)});
    }
    spec.appendPaths.push_back(std::move(subPath));
    return std::move(*this);
}

Prop Prop::prepend(std::string subPath, std::optional<std::string> matchOn) && {
    auto &spec = ensureMerge();
    if (matchOn) {
        spec.matchOn.push_back({.subPath = subPath, .key = std::move(*matchOn)});
    }
    spec.prependPaths.push_back(std::move(subPath));
    return std::move(*this);
}

Prop Prop::matchOn(std::string key) && {
    ensureMerge().matchOn.push_back({.subPath = "", .key = std::move(key)});
    return std::move(*this);
}

Prop Prop::once() && {
    ensureOnce();
    return std::move(*this);
}

Prop Prop::as(std::string key) && {
    ensureOnce().key = std::move(key);
    return std::move(*this);
}

Prop Prop::until(std::chrono::seconds ttl) && {
    ensureOnce().ttl = ttl;
    return std::move(*this);
}

Prop Prop::fresh(bool v) && {
    ensureOnce().fresh = v;
    return std::move(*this);
}

Prop Prop::merge() && {
    auto &spec = ensureMerge();
    if (spec.mode == MergeMode::Deep) {
        spec.mode = MergeMode::Append;
    }
    return std::move(*this);
}

Prop Prop::deepMerge() && {
    ensureMerge().mode = MergeMode::Deep;
    return std::move(*this);
}

Prop Prop::rescue() && {
    rescue_ = true;
    return std::move(*this);
}

Prop object(Props children) {
    Prop p{Json()};
    p.kind_ = Prop::Kind::Object;
    p.children_ = std::move(children);
    return p;
}

Prop optional(Resolver fn) {
    Prop p = std::move(fn);
    p.optional_ = true;
    return p;
}

Prop always(Prop p) {
    p.always_ = true;
    return p;
}

Prop defer(Resolver fn, std::string group) {
    Prop p = std::move(fn);
    p.deferGroup_ = std::move(group);
    return p;
}

Prop merge(Prop p) { return std::move(p).merge(); }

Prop deepMerge(Prop p) { return std::move(p).deepMerge(); }

Prop once(Resolver fn) { return Prop(std::move(fn)).once(); }

Prop scroll(Resolver fn, std::function<ScrollMetadata()> metadata, std::string wrapper) {
    Prop p = std::move(fn);
    p.scrollSpec_ = ScrollSpec{.wrapper = std::move(wrapper), .metadata = std::move(metadata)};
    return p;
}

} // namespace inertia
