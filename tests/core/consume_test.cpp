#include <catch2/catch_test_macros.hpp>

#include <inertia/detail/props_resolver.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <string>
#include <utility>

using namespace inertia;
using inertia::detail::ResolveContext;
using inertia::detail::resolveProps;

namespace {

std::string big() {
    std::string text;
    text.assign(4096, 'x');
    return text;
}

Props sample() {
    return {{"leaf", Json(big())},
            {"nested", object({{"inner", Json(big())}})},
            {"plain", Json::object({{"a", Json(big())}})}};
}

} // namespace

TEST_CASE("resolveProps on an rvalue moves the leaf values out", "[consume]") {
    auto props = sample();
    // The address of each heap string is taken before the move. A move keeps it; a copy would
    // allocate new storage.
    const auto *leaf = &props[0].second.value().get_ref<const std::string &>();
    const auto *inner =
        &props[1].second.children()[0].second.value().get_ref<const std::string &>();
    const auto *plain = &props[2].second.value()["a"].get_ref<const std::string &>();
    const auto resolved = resolveProps(std::move(props), ResolveContext{});
    REQUIRE(resolved.props["leaf"] == Json(big()));
    REQUIRE(resolved.props["nested"]["inner"] == Json(big()));
    REQUIRE(resolved.props["plain"]["a"] == Json(big()));
    REQUIRE(&resolved.props["leaf"].get_ref<const std::string &>() == leaf);
    REQUIRE(&resolved.props["nested"]["inner"].get_ref<const std::string &>() == inner);
    REQUIRE(&resolved.props["plain"]["a"].get_ref<const std::string &>() == plain);
}

TEST_CASE("resolveProps on an lvalue leaves the props untouched", "[consume]") {
    const auto props = sample();
    const auto resolved = resolveProps(props, ResolveContext{});
    REQUIRE(resolved.props["leaf"] == Json(big()));
    REQUIRE(props[0].second.value() == Json(big()));
    REQUIRE(props[1].second.children().size() == 1);
    REQUIRE(props[2].second.value()["a"] == Json(big()));
}
