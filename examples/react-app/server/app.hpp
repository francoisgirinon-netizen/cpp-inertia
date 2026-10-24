#pragma once

// What both executables share besides the routes: the port, the Vite integration and the
// configuration of the Inertia instance.

#include <inertia/inertia.hpp>
#include <inertia/ports/http_client.hpp>

#include <memory>
#include <string>

namespace example {

constexpr unsigned short kPort = 8081;
constexpr int kThreads = 4; // some demo resolvers sleep, so keep a few workers

// The directory holding the Vite build (`build/`) and the dev-server hot file.
[[nodiscard]] const std::string &publicDir();

// Builds the Inertia instance: Vite tags, asset version, SSR (INERTIA_SSR=1, with `bun run ssr`
// listening on :13714) and the shared props. Call it before any server thread starts: it reads the
// environment, and std::getenv is not safe against concurrent environment changes.
[[nodiscard]] std::shared_ptr<inertia::Inertia>
makeInertia(std::shared_ptr<inertia::HttpClient> client);

} // namespace example
