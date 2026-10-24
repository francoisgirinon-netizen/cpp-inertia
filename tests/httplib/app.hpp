#pragma once

#include "test_support.hpp"

#include <inertia/httplib/httplib.hpp>

#include <cstdint>
#include <memory>

namespace inertia::httplib_test {

// A running application for the adapter tests: Inertia (version "v1") installed on a cpp-httplib
// server with a few routes (see app.cpp). CSRF is on, with POST /echo exempt (it stands in for the
// SSR server, which sends no token).
class App {
  public:
    explicit App(inertia::httplib::Options options = defaultOptions());

    [[nodiscard]] TestServer &server() { return server_; }
    [[nodiscard]] std::uint16_t port() const { return server_.port(); }
    [[nodiscard]] Client client() const { return Client{server_.port()}; }
    [[nodiscard]] const std::shared_ptr<inertia::httplib::MemorySessionStore> &sessions() const {
        return sessions_;
    }

    [[nodiscard]] static inertia::httplib::Options defaultOptions();

  private:
    TestServer server_;
    std::shared_ptr<inertia::httplib::MemorySessionStore> sessions_;
};

} // namespace inertia::httplib_test
