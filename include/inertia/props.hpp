#pragma once

#include <inertia/json.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace inertia {

class Prop;
using Props = std::vector<std::pair<std::string, Prop>>;
using Resolver = std::function<Prop()>;

enum class MergeMode : std::uint8_t { Append, Prepend, Deep };

struct MatchOn {
    std::string subPath; // "" = the prop itself
    std::string key;
};

struct MergeSpec {
    MergeMode mode = MergeMode::Append;
    std::vector<std::string> appendPaths, prependPaths;
    std::vector<MatchOn> matchOn;
};

struct OnceSpec {
    std::optional<std::string> key;
    std::optional<std::chrono::seconds> ttl;
    bool fresh = false;
};

struct ScrollMetadata {
    std::string pageName = "page";
    Json previousPage = nullptr, nextPage = nullptr, currentPage = nullptr;
};

struct ScrollSpec {
    std::string wrapper = "data";
    std::function<ScrollMetadata()> metadata;
};

// A page prop: a plain JSON value, a lazy closure, or an object of nested props, optionally
// carrying a category (optional/always/defer) and metadata modifiers (merge/once/scroll).
// Values are immutable once handed to Inertia, so props can be shared across requests.
class Prop {
  public:
    enum class Kind : std::uint8_t { Value, Closure, Object };

    // Implicit by design: `{{"key", 1}}` and `{{"key", [] { ... }}}` must work in Props lists.
    // NOLINTBEGIN(cppcoreguidelines-explicit-constructor)
    template <class T>
        requires(!std::same_as<std::remove_cvref_t<T>, Prop> && !std::invocable<T &> &&
                 std::constructible_from<Json, T>)
    Prop(T &&v) : Prop(Json(std::forward<T>(v))) {}

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, Prop> && std::invocable<F &> &&
                 std::convertible_to<std::invoke_result_t<F &>, Prop>)
    Prop(F &&fn)
        : kind_(Kind::Closure),
          resolver_([f = std::forward<F>(fn)]() mutable -> Prop { return f(); }) {}

    Prop(Json v);
    // NOLINTEND(cppcoreguidelines-explicit-constructor)

    // Fluent modifiers.
    [[nodiscard]] Prop prepend() &&;
    [[nodiscard]] Prop append(std::string subPath, std::optional<std::string> matchOn = {}) &&;
    [[nodiscard]] Prop prepend(std::string subPath, std::optional<std::string> matchOn = {}) &&;
    [[nodiscard]] Prop matchOn(std::string key) &&;
    [[nodiscard]] Prop once() &&;
    [[nodiscard]] Prop as(std::string key) &&;
    [[nodiscard]] Prop until(std::chrono::seconds ttl) &&;
    [[nodiscard]] Prop fresh(bool v = true) &&;
    [[nodiscard]] Prop merge() &&;
    [[nodiscard]] Prop deepMerge() &&;
    [[nodiscard]] Prop rescue() &&;

    // Read access for the resolver.
    [[nodiscard]] Kind kind() const { return kind_; }
    [[nodiscard]] const Json &value() const { return value_; }
    [[nodiscard]] const Resolver &resolver() const { return resolver_; }
    [[nodiscard]] const Props &children() const { return children_; }
    [[nodiscard]] bool isOptional() const { return optional_; }
    [[nodiscard]] bool isAlways() const { return always_; }
    [[nodiscard]] const std::optional<std::string> &deferGroup() const { return deferGroup_; }
    [[nodiscard]] bool rescues() const { return rescue_; }
    [[nodiscard]] const std::optional<MergeSpec> &mergeSpec() const { return mergeSpec_; }
    [[nodiscard]] const std::optional<OnceSpec> &onceSpec() const { return onceSpec_; }
    [[nodiscard]] const std::optional<ScrollSpec> &scrollSpec() const { return scrollSpec_; }

  private:
    friend Prop object(Props children);
    friend Prop optional(Resolver fn);
    friend Prop always(Prop p);
    friend Prop defer(Resolver fn, std::string group);
    friend Prop scroll(Resolver fn, std::function<ScrollMetadata()> metadata, std::string wrapper);

    MergeSpec &ensureMerge();
    OnceSpec &ensureOnce();

    Kind kind_ = Kind::Value;
    Json value_;
    Resolver resolver_;
    Props children_;
    bool optional_ = false;
    bool always_ = false;
    bool rescue_ = false;
    std::optional<std::string> deferGroup_;
    std::optional<MergeSpec> mergeSpec_;
    std::optional<OnceSpec> onceSpec_;
    std::optional<ScrollSpec> scrollSpec_;
};

[[nodiscard]] Prop object(Props children); // nested props that may contain prop types
[[nodiscard]] Prop optional(Resolver fn);
[[nodiscard]] Prop always(Prop p);
[[nodiscard]] Prop defer(Resolver fn, std::string group = "default");
[[nodiscard]] Prop merge(Prop p);
[[nodiscard]] Prop deepMerge(Prop p);
[[nodiscard]] Prop once(Resolver fn);
[[nodiscard]] Prop scroll(Resolver fn, std::function<ScrollMetadata()> metadata,
                          std::string wrapper = "data");

} // namespace inertia
