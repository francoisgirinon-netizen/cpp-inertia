#include <catch2/catch_test_macros.hpp>

#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

TEST_CASE("install refuses to run twice", "[drogon]") {
    // The test server already called install() once.
    inertia::Config config;
    config.version = [] { return std::string("v1"); };
    CHECK_THROWS_AS(inertia::drogon::install(std::make_shared<inertia::Inertia>(std::move(config))),
                    std::logic_error);
}
