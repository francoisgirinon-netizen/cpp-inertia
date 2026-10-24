#include <catch2/catch_test_macros.hpp>

#include <inertia/detail/props_resolver.hpp>
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
    // Consumed through a reference on purpose: the test then inspects the moved-from source.
    Props &consumed = props;
    const auto resolved = resolveProps(std::move(consumed), ResolveContext{});
    REQUIRE(resolved.props["leaf"] == Json(big()));
    REQUIRE(resolved.props["nested"]["inner"] == Json(big()));
    REQUIRE(resolved.props["plain"]["a"] == Json(big()));
    // Moved-from JSON is null; a copy would have left the source intact.
    for (const auto &[key, prop] : props) {
        INFO(key);
        REQUIRE(prop.value().is_null());
        REQUIRE(prop.children().empty());
    }
}

TEST_CASE("resolveProps on an lvalue leaves the props untouched", "[consume]") {
    const auto props = sample();
    const auto resolved = resolveProps(props, ResolveContext{});
    REQUIRE(resolved.props["leaf"] == Json(big()));
    REQUIRE(props[0].second.value() == Json(big()));
    REQUIRE(props[1].second.children().size() == 1);
    REQUIRE(props[2].second.value()["a"] == Json(big()));
}
