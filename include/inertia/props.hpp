#pragma once

#include <inertia/json.hpp>

#include <chrono>
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#include <concepts>
#endif
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace inertia {

// The Prop constructors are written twice, with the same semantics: C++20 concepts for the
// library's own build, enable_if for C++17 consumers (the headers must compile as C++17).
#if !(defined(__cpp_concepts) && __cpp_concepts >= 201907L && defined(__cpp_conditional_explicit))
namespace detail {
template <class T> using RemoveCvRef = std::remove_cv_t<std::remove_reference_t<T>>;
} // namespace detail
#endif

class Prop;

/// Named page props, in the order they are emitted.
///
/// A key may be a dot path such as `"auth.name"`, which addresses a nested value. A top-level dot
/// key is unpacked into nested objects before the props are resolved.
using Props = std::vector<std::pair<std::string, Prop>>;

/// Callable that produces a prop's value. It runs only when the prop is resolved, which the
/// prop's category (optional, defer, once, ...) can postpone or skip altogether.
using Resolver = std::function<Prop()>;

/// How the client combines a merged prop's new value with the value it already holds.
enum class MergeMode : std::uint8_t {
    /// New array items are appended to the client's current array.
    Append,
    /// New array items are prepended to the client's current array.
    Prepend,
    /// Objects are merged recursively, key by key.
    Deep,
};

/// Identifies the items of a merged array so that the client updates an existing item in place
/// instead of adding a duplicate.
struct MatchOn {
    /// Path of the array relative to the prop, or empty when the prop itself is the array.
    std::string subPath;
    /// Item field that identifies an item, such as `"id"`.
    std::string key;
};

/// Merge settings of a prop, reported to the client as `mergeProps`, `prependProps`,
/// `deepMergeProps` and `matchPropsOn`.
struct MergeSpec {
    /// Mode of the prop itself.
    MergeMode mode = MergeMode::Append;
    /// Nested paths merged by appending, relative to the prop, such as `"data"`.
    std::vector<std::string> appendPaths;
    /// Nested paths merged by prepending, relative to the prop, such as `"meta"`.
    std::vector<std::string> prependPaths;
    /// Item-matching rules for the prop and its nested paths.
    std::vector<MatchOn> matchOn;
};

/// Once-prop settings. The server omits the value from later visits while the client still holds
/// it, and the client remembers it under `key`.
struct OnceSpec {
    /// Key the client remembers the value under. Unset means the prop path.
    std::optional<std::string> key;
    /// Lifetime of the client's copy, reported as `expiresAt` (now plus ttl). Unset means no
    /// expiry.
    std::optional<std::chrono::seconds> ttl;
    /// Resolve and send the value on every full visit, even when the client already holds it.
    bool fresh = false;
};

/// Paging state of an infinite-scroll prop, reported to the client as `scrollProps`.
struct ScrollMetadata {
    /// Name of the query parameter that requests a page.
    std::string pageName = "page";
    /// Page number or cursor of the previous page. `null` when there is no such page.
    Json previousPage = nullptr;
    /// Page number or cursor of the next page. `null` when there is no such page.
    Json nextPage = nullptr;
    /// Page number or cursor of the current page.
    Json currentPage = nullptr;
};

/// Infinite-scroll settings of a prop.
struct ScrollSpec {
    /// Key, inside the resolved value, that holds the items. The merge path is
    /// `"<prop>.<wrapper>"`.
    std::string wrapper = "data";
    /// Returns the paging state for the response. Empty means a default `ScrollMetadata`.
    std::function<ScrollMetadata()> metadata;
};

/// A page prop: a plain JSON value, a lazy closure, or an object of nested props. It may also carry
/// a category (optional, always, defer) and metadata modifiers (merge, once, scroll).
///
/// Build props with the constructors and factory functions below, then pass them to
/// Inertia::share() or Inertia::render(). Inertia takes ownership of the prop and never changes
/// it, so a prop can be built once and reused across requests. The modifiers consume the prop
/// they are called on, so they chain on a temporary:
///
/// @code{.cpp}
/// inertia::Props props{
///     {"user", user},                                            // always sent
///     {"stats", inertia::optional([] { return loadStats(); })},  // partial reloads only
///     {"plans", inertia::once([] { return loadPlans(); }).until(std::chrono::hours{1})},
///     {"feed", inertia::defer([] { return loadFeed(); }).merge()},
/// };
/// @endcode
class Prop {
  public:
    /// Which of the three forms a prop holds.
    enum class Kind : std::uint8_t {
        /// A JSON value; see value().
        Value,
        /// A closure that yields the value on resolution; see resolver().
        Closure,
        /// An object of nested props; see children().
        Object,
    };

    /// Wraps any JSON-convertible value. Implicit so that a Props list can be written as
    /// `{{"key", 1}}`.
    /// @tparam T Value type convertible to Json.
    /// @param v Value to send as the prop.
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L && defined(__cpp_conditional_explicit)
    template <class T>
        requires(!std::same_as<std::remove_cvref_t<T>, Prop> && !std::invocable<T &> &&
                 std::constructible_from<Json, T>)
    explicit(false) Prop(T &&v)
#else
    template <class T,
              std::enable_if_t<!std::is_same_v<detail::RemoveCvRef<T>, Prop> &&
                                   !std::is_invocable_v<T &> && std::is_constructible_v<Json, T>,
                               int> = 0>
    Prop(T &&v)
#endif
        : Prop(Json(std::forward<T>(v))) {
    }

    /// Wraps a nullary callable that returns a Prop-convertible value. Implicit so that a Props
    /// list can be written as `{{"key", [] { ... }}}`. The callable runs during the page build,
    /// on the thread that calls Inertia::render() or Inertia::page().
    /// @tparam F Callable type with no arguments.
    /// @param fn Callable that produces the value.
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L && defined(__cpp_conditional_explicit)
    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, Prop> && std::invocable<F &> &&
                 std::convertible_to<std::invoke_result_t<F &>, Prop>)
    explicit(false) Prop(F &&fn)
#else
    template <class F, std::enable_if_t<!std::is_same_v<detail::RemoveCvRef<F>, Prop> &&
                                            std::is_invocable_v<F &> &&
                                            std::is_convertible_v<std::invoke_result_t<F &>, Prop>,
                                        int> = 0>
    Prop(F &&fn)
#endif
        : kind_(Kind::Closure),
          resolver_([f = std::forward<F>(fn)]() mutable -> Prop { return f(); }) {
    }

    /// Wraps an already-built JSON value.
    /// @param v Value to send as the prop.
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L && defined(__cpp_conditional_explicit)
    explicit(false) Prop(Json v);
#else
    Prop(Json v);
#endif

    /// Marks a merged prop as prepended: the client puts new array items before its current ones.
    /// @return The modified prop.
    [[nodiscard]] Prop prepend() &&;

    /// Merges the array at a nested path by appending, and optionally matches its items by key.
    /// Call once per nested path.
    /// @param subPath Path of the array relative to this prop, such as `"data"`.
    /// @param matchOn Item field that identifies an item, such as `"id"`. Empty means no matching.
    /// @return The modified prop.
    [[nodiscard]] Prop append(std::string subPath, std::optional<std::string> matchOn = {}) &&;

    /// Merges the array at a nested path by prepending, and optionally matches its items by key.
    /// Call once per nested path.
    /// @param subPath Path of the array relative to this prop, such as `"meta"`.
    /// @param matchOn Item field that identifies an item, such as `"id"`. Empty means no matching.
    /// @return The modified prop.
    [[nodiscard]] Prop prepend(std::string subPath, std::optional<std::string> matchOn = {}) &&;

    /// Matches the items of this merged prop by a key, so that an item the client already holds
    /// is updated instead of duplicated.
    /// @param key Item field that identifies an item, such as `"id"`.
    /// @return The modified prop.
    [[nodiscard]] Prop matchOn(std::string key) &&;

    /// Makes the prop a once prop. On a full visit the server omits the value while the client
    /// holds it, and reports it under `onceProps` instead. See also as(), until() and fresh().
    /// @return The modified prop.
    [[nodiscard]] Prop once() &&;

    /// Sets the key the client remembers a once prop under. Use it to share one client-side copy
    /// between props. Defaults to the prop path.
    /// @param key Name of the once value on the client.
    /// @return The modified prop.
    [[nodiscard]] Prop as(std::string key) &&;

    /// Sets how long the client may keep a once value. It is reported as `expiresAt`, computed as
    /// the current time plus `ttl`. Without a ttl, `expiresAt` is `null`.
    /// @param ttl Lifetime of the client's copy.
    /// @return The modified prop.
    [[nodiscard]] Prop until(std::chrono::seconds ttl) &&;

    /// Controls whether a once prop is always resolved and sent on a full visit, even when the
    /// client already holds it.
    /// @param v `true` to always send the value, `false` to restore the default.
    /// @return The modified prop.
    [[nodiscard]] Prop fresh(bool v = true) &&;

    /// Marks the prop as merged by appending. On partial reloads the client appends new array
    /// items to its current array. A prepend() mode is kept, and deepMerge() is replaced.
    /// @return The modified prop.
    [[nodiscard]] Prop merge() &&;

    /// Marks the prop as deep-merged: on partial reloads the client merges objects key by key
    /// instead of replacing them.
    /// @return The modified prop.
    [[nodiscard]] Prop deepMerge() &&;

    /// For a deferred prop: if its resolver throws, the response still succeeds without the prop.
    /// The prop's path is then listed in `rescuedProps`.
    /// @return The modified prop.
    [[nodiscard]] Prop rescue() &&;

    /// Marks the prop as deferred. It is left out of a full visit and listed under
    /// `deferredProps[group]`. The client loads it with a follow-up request.
    /// @param group Name of the group the client requests together. Defaults to `"default"`.
    /// @return The modified prop.
    [[nodiscard]] Prop deferred(std::string group = "default") &&;

    /// Returns a copy of this prop with its nested props replaced. The category and the modifiers
    /// are kept, and the result is an object prop.
    /// @param children Nested props.
    /// @return The new prop.
    [[nodiscard]] Prop withChildren(Props children) const;

    /// Returns a copy of this prop with its closure replaced. The category and the modifiers are
    /// kept, and the result is a closure prop.
    /// @param fn Closure that produces the new value.
    /// @return The new prop.
    [[nodiscard]] Prop withResolver(Resolver fn) const;

    /// @return Which form the prop holds.
    [[nodiscard]] Kind kind() const { return kind_; }

    /// @return The JSON value. Null for closure and object props.
    [[nodiscard]] const Json &value() const { return value_; }

    /// @return The closure. Empty unless kind() is Kind::Closure.
    [[nodiscard]] const Resolver &resolver() const { return resolver_; }

    /// @return The nested props. Empty unless kind() is Kind::Object.
    [[nodiscard]] const Props &children() const { return children_; }

    /// Moves the JSON value out of a prop that is being consumed. The prop is left in a
    /// moved-from state and must not be read afterwards.
    /// @return The JSON value.
    [[nodiscard]] Json takeValue() && { return std::move(value_); }

    /// Moves the nested props out of a prop that is being consumed. The prop is left in a
    /// moved-from state and must not be read afterwards.
    /// @return The nested props.
    [[nodiscard]] Props takeChildren() && { return std::move(children_); }

    /// @return `true` for an optional() prop, which is resolved only when a partial reload selects
    /// it.
    [[nodiscard]] bool isOptional() const { return optional_; }

    /// @return `true` for an always() prop, which is resolved on every response, including partial
    /// reloads that do not select it.
    [[nodiscard]] bool isAlways() const { return always_; }

    /// @return The group the prop is deferred into, or empty when the prop is not deferred.
    [[nodiscard]] const std::optional<std::string> &deferGroup() const { return deferGroup_; }

    /// @return `true` when rescue() was set.
    [[nodiscard]] bool rescues() const { return rescue_; }

    /// @return The merge settings, or empty when the prop is not merged.
    [[nodiscard]] const std::optional<MergeSpec> &mergeSpec() const { return mergeSpec_; }

    /// @return The once settings, or empty when the prop is not a once prop.
    [[nodiscard]] const std::optional<OnceSpec> &onceSpec() const { return onceSpec_; }

    /// @return The infinite-scroll settings, or empty when the prop is not a scroll prop.
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

/// Groups nested props under one key. The children may be props themselves, so the modifiers
/// apply at any depth.
/// @param children Nested props.
/// @return An object prop.
[[nodiscard]] Prop object(Props children);

/// Creates an optional prop. Full visits skip it, and it is resolved only when a partial reload
/// selects it. Use it for data that is expensive and rarely needed.
/// @param fn Callable that produces the value.
/// @return The optional prop.
[[nodiscard]] Prop optional(Resolver fn);

/// Creates an always prop. The value is sent on every response, including partial reloads that
/// do not select it.
/// @param p Prop to mark as always.
/// @return The modified prop.
[[nodiscard]] Prop always(Prop p);

/// Creates a deferred prop. A full visit omits it, and the client loads it with a follow-up
/// request for its group. Props that share a group are loaded together.
/// @param fn Callable that produces the value.
/// @param group Name of the group the client requests together. Defaults to `"default"`.
/// @return The deferred prop.
[[nodiscard]] Prop defer(Resolver fn, std::string group = "default");

/// Shorthand for `p.merge()`.
/// @param p Prop to merge.
/// @return The merged prop.
[[nodiscard]] Prop merge(Prop p);

/// Shorthand for `p.deepMerge()`.
/// @param p Prop to deep-merge.
/// @return The deep-merged prop.
[[nodiscard]] Prop deepMerge(Prop p);

/// Creates a once prop. Shorthand for `Prop(fn).once()`.
/// @param fn Callable that produces the value.
/// @return The once prop.
[[nodiscard]] Prop once(Resolver fn);

/// Creates an infinite-scroll prop. Each response carries one page of items. The client appends
/// them to its list, or prepends them when the request sends the merge-intent header `prepend`.
/// @param fn Callable that produces the page, with the items under `wrapper`.
/// @param metadata Callable that produces the paging state for the response.
/// @param wrapper Key, inside the value of `fn`, that holds the items.
/// @return The scroll prop.
[[nodiscard]] Prop scroll(Resolver fn, std::function<ScrollMetadata()> metadata,
                          std::string wrapper = "data");

} // namespace inertia
