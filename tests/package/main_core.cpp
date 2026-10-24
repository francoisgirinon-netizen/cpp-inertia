// Consumer of the installed package, components core + fake: renders one page object.
#include <inertia/fake/fake.hpp>
#include <inertia/inertia.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    const inertia::Inertia app{inertia::Config{}};
    inertia::fake::MemorySession session;
    const auto page =
        app.page(inertia::fake::FakeRequest::inertia("/users"), session, "Users/Index", {{"a", 1}});
    const std::string expected =
        R"({"component":"Users/Index","props":{"a":1,"errors":{}},"url":"/users","version":""})";
    if (inertia::dumpJson(page) != expected) {
        std::cerr << "unexpected page: " << inertia::dumpJson(page) << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
