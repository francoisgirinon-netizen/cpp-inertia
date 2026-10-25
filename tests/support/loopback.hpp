#pragma once

#include <cstdint>
#include <string>

// Blocking TCP helpers on the loopback interface for the tests, over BSD sockets on Linux and
// macOS and Winsock on Windows. The sockets are only here for what the frameworks' own clients
// cannot do: pick a free port, poll for a listener, and send bytes verbatim.
namespace inertia::test_loopback {

// Asks the system for a free port on 127.0.0.1. ctest starts one process per test case (and may run
// them in parallel), so a fixed port would collide; a probed port cannot (short of a race with an
// unrelated process). Throws std::runtime_error on failure.
[[nodiscard]] std::uint16_t probeFreePort();

// Whether something accepts a TCP connection on 127.0.0.1:`port` right now.
[[nodiscard]] bool acceptsConnections(std::uint16_t port);

// Sends `request` to 127.0.0.1:`port` exactly as given and returns everything the peer sends back
// until it closes the connection. Gives up after 15 s without data. Throws std::runtime_error.
[[nodiscard]] std::string rawExchange(std::uint16_t port, const std::string &request);

} // namespace inertia::test_loopback
