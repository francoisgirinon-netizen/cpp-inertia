#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/html.hpp>
#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace inertia;
using namespace inertia::fake;

namespace {

std::string propsOf(const Props &props) {
    const Inertia app{Config{}};
    MemorySession session;
    return dumpJson(app.page(FakeRequest::inertia("/"), session, "Page", props)["props"]);
}

} // namespace

TEST_CASE("plain values become value props", "[props]") {
    const Prop number = 1;
    REQUIRE(number.kind() == Prop::Kind::Value);
    REQUIRE(number.value() == Json(1));
    const Prop text = "hello";
    REQUIRE(text.value() == Json("hello"));
    const Prop json = Json::object({{"a", 1}});
    REQUIRE(json.kind() == Prop::Kind::Value);
    REQUIRE(json.value() == Json::object({{"a", 1}}));
}

TEST_CASE("callables become lazy closures", "[props]") {
    int calls = 0;
    const Prop lazy = [&calls] {
        ++calls;
        return 5;
    };
    REQUIRE(lazy.kind() == Prop::Kind::Closure);
    REQUIRE(calls == 0);
    const Prop resolved = lazy.resolver()();
    REQUIRE(calls == 1);
    REQUIRE(resolved.value() == Json(5));
}

TEST_CASE("props copy and keep their kind", "[props]") {
    const Prop original = object({{"a", 1}});
    const Prop copy = always(original);
    REQUIRE(copy.kind() == Prop::Kind::Object);
    REQUIRE(copy.isAlways());
    REQUIRE_FALSE(original.isAlways());
    REQUIRE(copy.children().size() == 1);
    REQUIRE(copy.children().front().first == "a");
}

TEST_CASE("factories set the category", "[props]") {
    REQUIRE(optional([] { return 1; }).isOptional());
    REQUIRE(always(1).isAlways());
    REQUIRE_FALSE(Prop{1}.isOptional());
    REQUIRE_FALSE(Prop{1}.isAlways());
    const Prop deferred = defer([] { return 1; }, "side");
    REQUIRE(deferred.deferGroup() == "side");
    REQUIRE(defer([] { return 1; }).deferGroup() == "default");
    REQUIRE_FALSE(Prop{1}.deferGroup().has_value());
}

TEST_CASE("merge modifiers build the merge spec", "[props]") {
    REQUIRE_FALSE(Prop{1}.mergeSpec().has_value());
    REQUIRE(merge(1).mergeSpec().value_or(MergeSpec{}).mode == MergeMode::Append);
    REQUIRE(deepMerge(1).mergeSpec().value_or(MergeSpec{}).mode == MergeMode::Deep);
    REQUIRE(Prop{1}.merge().prepend().mergeSpec().value_or(MergeSpec{}).mode == MergeMode::Prepend);

    const auto spec =
        Prop{1}.merge().append("items", "id").prepend("log").matchOn("uuid").mergeSpec();
    REQUIRE(spec.has_value());
    const auto merged = spec.value_or(MergeSpec{});
    REQUIRE(merged.appendPaths == std::vector<std::string>{"items"});
    REQUIRE(merged.prependPaths == std::vector<std::string>{"log"});
    REQUIRE(merged.matchOn.size() == 2);
    REQUIRE(merged.matchOn[0].subPath == "items");
    REQUIRE(merged.matchOn[0].key == "id");
    REQUIRE(merged.matchOn[1].subPath.empty());
    REQUIRE(merged.matchOn[1].key == "uuid");
}

TEST_CASE("once modifiers build the once spec", "[props]") {
    REQUIRE_FALSE(Prop{1}.onceSpec().has_value());
    const auto spec =
        once([] { return 1; }).as("k").until(std::chrono::seconds(60)).fresh().onceSpec();
    REQUIRE(spec.has_value());
    const auto onced = spec.value_or(OnceSpec{});
    REQUIRE(onced.key == "k");
    REQUIRE(onced.ttl == std::chrono::seconds(60));
    REQUIRE(onced.fresh);
    REQUIRE_FALSE(once([] { return 1; }).onceSpec().value_or(OnceSpec{}).fresh);
}

TEST_CASE("scroll and rescue modifiers", "[props]") {
    const auto scrolled = scroll([] { return 1; }, [] { return ScrollMetadata{}; }, "rows");
    REQUIRE(scrolled.scrollSpec().has_value());
    REQUIRE(scrolled.scrollSpec().value_or(ScrollSpec{}).wrapper == "rows");
    REQUIRE_FALSE(Prop{1}.rescues());
    REQUIRE(defer([] { return 1; }).rescue().rescues());
}

TEST_CASE("top-level dot keys unpack into nested objects", "[props][protocol:props.dot-keys]") {
    REQUIRE(propsOf({{"a.b", 1}, {"a.c", 2}}) == R"({"a":{"b":1,"c":2},"errors":{}})");
    REQUIRE(propsOf({{"a.b.c", 1}, {"a.b.d", 2}, {"e", 3}}) ==
            R"({"a":{"b":{"c":1,"d":2}},"e":3,"errors":{}})");
}

TEST_CASE("nested dot keys are not unpacked", "[props][protocol:props.dot-keys]") {
    REQUIRE(propsOf({{"a", object({{"x.y", 1}})}}) == R"({"a":{"x.y":1},"errors":{}})");
}

TEST_CASE("a later plain key replaces an earlier one in place", "[props]") {
    REQUIRE(propsOf({{"a", 1}, {"b", 2}, {"a", 3}}) == R"({"a":3,"b":2,"errors":{}})");
}

TEST_CASE("closures resolve lazily and nested", "[props]") {
    REQUIRE(propsOf({{"a", [] { return object({{"n", [] { return 1; }}}); }}}) ==
            R"({"a":{"n":1},"errors":{}})");
}

TEST_CASE("plain closures resolve on full visits", "[props]") {
    int calls = 0;
    const auto out = propsOf({{"a", [&calls] {
                                   ++calls;
                                   return Json::array({1, 2});
                               }}});
    REQUIRE(out == R"({"a":[1,2],"errors":{}})");
    REQUIRE(calls == 1);
}
