#include <catch2/catch_test_macros.hpp>

#include <inertia/headers.hpp>
#include <inertia/response.hpp>

#include <optional>
#include <string>
#include <vector>

using inertia::Response;
using inertia::headers::parseList;

TEST_CASE("parseList trims and drops empties", "[headers]") {
    REQUIRE(parseList(std::optional<std::string>{" a , ,b "}) ==
            std::vector<std::string>{"a", "b"});
}

TEST_CASE("parseList of only commas is empty", "[headers]") {
    REQUIRE(parseList(std::optional<std::string>{",,"}).empty());
}

TEST_CASE("parseList nullopt returns empty list", "[headers]") {
    REQUIRE(parseList(std::nullopt).empty());
}

TEST_CASE("Response header lookup is case-insensitive", "[headers]") {
    Response response;
    response.setHeader("Vary", "X");
    REQUIRE(response.header("vary").value_or("") == "X");

    response.setHeader("VARY", "Y");
    REQUIRE(response.headers.size() == 1);
    REQUIRE(response.header("vary").value_or("") == "Y");
}
