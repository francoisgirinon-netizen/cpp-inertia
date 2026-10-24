#include <catch2/catch_test_macros.hpp>

#include <inertia/json.hpp>

#include <string>

TEST_CASE("Json keeps insertion order", "[smoke]") {
    inertia::Json j;
    j["b"] = 1;
    j["a"] = 2;
    REQUIRE(j.dump() == R"({"b":1,"a":2})");
}
