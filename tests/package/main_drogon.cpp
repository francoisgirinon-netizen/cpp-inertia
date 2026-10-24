// Consumer of the installed package, component drogon: links the adapter's translation units
// (install() wires the advices into the Drogon app, nothing is served).
#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>

#include <cstdlib>
#include <memory>

int main() {
    inertia::drogon::DrogonHttpClient client;
    inertia::drogon::install(std::make_shared<const inertia::Inertia>(inertia::Config{}));
    return EXIT_SUCCESS;
}
