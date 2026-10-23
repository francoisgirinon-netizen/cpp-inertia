#include <catch2/catch_test_macros.hpp>

#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>

#include <string>

using namespace inertia;
using namespace inertia::fake;

namespace {

// Builds the partial-reload request headers.
class Partial {
  public:
    [[nodiscard]] Partial only(std::string v) && {
        only_ = std::move(v);
        return std::move(*this);
    }
    [[nodiscard]] Partial except(std::string v) && {
        except_ = std::move(v);
        return std::move(*this);
    }
    [[nodiscard]] Partial component(std::string v) && {
        component_ = std::move(v);
        return std::move(*this);
    }
    [[nodiscard]] FakeRequest request() const {
        auto req = FakeRequest::inertia("/");
        req.with("X-Inertia-Partial-Component", component_);
        if (!only_.empty()) {
            req.with("X-Inertia-Partial-Data", only_);
        }
        if (!except_.empty()) {
            req.with("X-Inertia-Partial-Except", except_);
        }
        return req;
    }

  private:
    std::string only_, except_, component_ = "Page";
};

// Renders "Page" for a partial request and returns the dumped props.
std::string partial(const Props &props, const Partial &headers) {
    const Inertia app{Config{}};
    MemorySession session;
    return dumpJson(app.page(headers.request(), session, "Page", props)["props"]);
}

Props authProps() {
    return {{"auth", object({{"user", Json::object({{"id", 1}})}, {"token", "t"}})}, {"other", 2}};
}

} // namespace

TEST_CASE("only selects a nested path", "[partial][protocol:partial.only]") {
    REQUIRE(partial(authProps(), Partial{}.only("auth.user")) ==
            R"({"auth":{"user":{"id":1}},"errors":{}})");
}

TEST_CASE("only with an ancestor path selects the whole subtree",
          "[partial][protocol:partial.only]") {
    REQUIRE(partial(authProps(), Partial{}.only("auth")) ==
            R"({"auth":{"user":{"id":1},"token":"t"},"errors":{}})");
}

TEST_CASE("only accepts several comma separated paths", "[partial][protocol:partial.only]") {
    REQUIRE(partial(authProps(), Partial{}.only("auth.token, other")) ==
            R"({"auth":{"token":"t"},"other":2,"errors":{}})");
}

TEST_CASE("only does not select by prefix of a segment", "[partial][protocol:partial.only]") {
    REQUIRE(partial({{"auth", 1}, {"authority", 2}}, Partial{}.only("auth")) ==
            R"({"auth":1,"errors":{}})");
}

TEST_CASE("only on a deep path keeps that path and its ancestors",
          "[partial][protocol:partial.dot-notation]") {
    const Props props{{"a.b.c", 1}, {"a.b.d", 2}, {"a.e", 3}, {"f", 4}};
    REQUIRE(partial(props, Partial{}.only("a.b.c")) == R"({"a":{"b":{"c":1}},"errors":{}})");
}

TEST_CASE("except removes a path and keeps its siblings", "[partial][protocol:partial.except]") {
    REQUIRE(partial(authProps(), Partial{}.except("auth.token")) ==
            R"({"auth":{"user":{"id":1}},"other":2,"errors":{}})");
}

TEST_CASE("except removes descendants and wins over only", "[partial][protocol:partial.except]") {
    REQUIRE(partial(authProps(), Partial{}.only("auth").except("auth")) == R"({"errors":{}})");
    REQUIRE(partial(authProps(), Partial{}.only("auth").except("auth.token")) ==
            R"({"auth":{"user":{"id":1}},"errors":{}})");
}

TEST_CASE("a partial request for another component is a full visit",
          "[partial][protocol:partial.component-match]") {
    REQUIRE(partial(authProps(), Partial{}.only("other").component("Other")) ==
            R"({"auth":{"user":{"id":1},"token":"t"},"other":2,"errors":{}})");
}

TEST_CASE("partial headers without the component header are ignored",
          "[partial][protocol:partial.component-match]") {
    const Inertia app{Config{}};
    MemorySession session;
    auto req = FakeRequest::inertia("/");
    req.with("X-Inertia-Partial-Data", "other");
    REQUIRE(dumpJson(app.page(req, session, "Page", authProps())["props"]) ==
            R"({"auth":{"user":{"id":1},"token":"t"},"other":2,"errors":{}})");
}

TEST_CASE("a closure's resolved children bypass the filter",
          "[partial][protocol:partial.resolved-children]") {
    const Props props{{"a", [] { return Json::object({{"x", 1}, {"y", 2}}); }}, {"b", 3}};
    REQUIRE(partial(props, Partial{}.only("a.x")) == R"({"a":{"x":1,"y":2},"errors":{}})");
}

TEST_CASE("children of a resolved object closure bypass the filter",
          "[partial][protocol:partial.resolved-children]") {
    const Props props{{"a", [] { return object({{"x", 1}, {"y", 2}}); }}};
    REQUIRE(partial(props, Partial{}.only("a.x")) == R"({"a":{"x":1,"y":2},"errors":{}})");
}

TEST_CASE("closures that are not selected are never invoked", "[partial]") {
    int calls = 0;
    const Props props{{"a",
                       [&calls] {
                           ++calls;
                           return 1;
                       }},
                      {"b", 2}};
    REQUIRE(partial(props, Partial{}.only("b")) == R"({"b":2,"errors":{}})");
    REQUIRE(calls == 0);
    REQUIRE(partial(props, Partial{}.only("a")) == R"({"a":1,"errors":{}})");
    REQUIRE(calls == 1);
}
