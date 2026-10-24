#include <catch2/catch_test_macros.hpp>

#include <inertia/headers.hpp>
#include <inertia/response.hpp>

#include <optional>
#include <string>
#include <utility>
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

TEST_CASE("parseList edge lengths: empty, blank, single item and trailing separators",
          "[headers]") {
    REQUIRE(parseList(std::optional<std::string>{""}).empty());
    REQUIRE(parseList(std::optional<std::string>{" \t\r\n "}).empty());
    REQUIRE(parseList(std::optional<std::string>{"a"}) == std::vector<std::string>{"a"});
    REQUIRE(parseList(std::optional<std::string>{" a "}) == std::vector<std::string>{"a"});
    REQUIRE(parseList(std::optional<std::string>{"a,"}) == std::vector<std::string>{"a"});
    REQUIRE(parseList(std::optional<std::string>{",a"}) == std::vector<std::string>{"a"});
    REQUIRE(parseList(std::optional<std::string>{"a,,"}) == std::vector<std::string>{"a"});
    REQUIRE(parseList(std::optional<std::string>{"a,b"}) == std::vector<std::string>{"a", "b"});
    REQUIRE(parseList(std::optional<std::string>{"a, ,"}) == std::vector<std::string>{"a"});
}

TEST_CASE("parseList keeps the last item when the text ends right after it", "[headers]") {
    REQUIRE(parseList(std::optional<std::string>{"x,y,z"}) ==
            std::vector<std::string>{"x", "y", "z"});
    REQUIRE(parseList(std::optional<std::string>{"x,y,\tz\t"}) ==
            std::vector<std::string>{"x", "y", "z"});
}

TEST_CASE("Response header lookup is case-insensitive", "[headers]") {
    Response response;
    response.setHeader("Vary", "X");
    REQUIRE(response.header("vary").value_or("") == "X");

    response.setHeader("VARY", "Y");
    REQUIRE(response.headers.size() == 1);
    REQUIRE(response.header("vary").value_or("") == "Y");
}
