# cpp-inertia

A C++ implementation of the server side of the [Inertia.js](https://inertiajs.com) v3 protocol.

- **Framework-agnostic core** (`inertia::core`): pure C++ that depends only on nlohmann_json. It builds the page object, resolves props (partial reloads, deferred, merge, once, scroll, ...), renders the first-visit HTML and talks to an SSR server.
- **Drogon adapter** (`inertia::drogon`): one `install()` call and a `render()` helper.
- **cpp-httplib adapter** (`inertia::httplib`): the same shape for [cpp-httplib](https://github.com/yhirose/cpp-httplib), with its own in-memory sessions and a blocking SSR client. Optional, like Drogon.
- **Fake adapter** (`inertia::fake`): in-memory, for tests.
- **React example** (`examples/react-app`) driven by Playwright with the official client.

Status: every rule of the Inertia v3 server protocol is listed in a [coverage matrix](docs/protocol-coverage.toml), pinned to `@inertiajs/react` **3.9.1**, and each one is either proven by a tagged test or explicitly out of scope (see [Protocol coverage](#protocol-coverage)). Version 0.1.0: the API may still change.

## Quick start (Docker)

Everything (clang 23, CMake, Ninja, vcpkg, bun, Playwright) lives in one image, so the host needs only Docker.

```sh
docker compose build                              # once
scripts/dev cmake --workflow --preset dev         # configure + build + test
scripts/dev bash                                  # an interactive shell in the container
```

- `scripts/dev <cmd...>` runs `<cmd>` in the `cpp-inertia-dev` container as your UID/GID, with the repository mounted at `/src`. Hooks and builds go through it.
- `scripts/dev-serve <cmd...>` is the same with the ports published (8081 Drogon, 5173 Vite, 13714 SSR). Use it for anything that serves. The published ports bind to 127.0.0.1 only, so they are not reachable from the LAN; the example is at http://127.0.0.1:8081.
- `scripts/dev-tsan <cmd...>` uses a service with a relaxed seccomp profile, needed only by ThreadSanitizer: `scripts/dev-tsan cmake --workflow --preset tsan`.

Run the example application:

```sh
# production mode: build the server and the front end, then serve
scripts/dev sh -c 'cmake --preset dev -DINERTIA_BUILD_EXAMPLES=ON && cmake --build --preset dev \
  && cd examples/react-app/frontend && bun install && bun run build'
scripts/dev-serve build/dev/examples/react-app/server/inertia_example      # http://127.0.0.1:8081

# development mode with hot reload: Vite writes public/hot, the server then emits dev tags
scripts/dev-serve sh -c 'cd examples/react-app/frontend && bun run dev'    # in one terminal
scripts/dev-serve build/dev/examples/react-app/server/inertia_example      # in another

# server-side rendering: the SSR server (:13714) must share the container's localhost with the
# example, so start both from one command (INERTIA_SSR is forwarded from the host)
INERTIA_SSR=1 scripts/dev-serve sh -c '(cd examples/react-app/frontend && bun run ssr) & \
  exec build/dev/examples/react-app/server/inertia_example'
```

The same pages also run on the cpp-httplib adapter. `inertia_example` (Drogon) and `inertia_example_httplib` (cpp-httplib) share the routes (`examples/react-app/server/*.cpp`, written against the inertia core and the small `Context`/`Router` interfaces of `context.hpp`); only `drogon_main.cpp` and `httplib_main.cpp` differ, each a thin registration layer. Both listen on port 8081, so run one at a time, with any of the commands above:

```sh
scripts/dev-serve build/dev/examples/react-app/server/inertia_example_httplib   # http://127.0.0.1:8081
```

`INERTIA_BUILD_EXAMPLES` needs at least one of the two adapters; each executable is built when its adapter is.

### Dev container

`.devcontainer/devcontainer.json` opens the same `dev` service of `compose.yaml` as a dev container (VS Code "Reopen in Container", CLion "Dev Containers", or `devcontainer up --workspace-folder .`). The sources are at `/src`, the toolchain, vcpkg, bun and sccache volumes are the ones `scripts/dev` uses, and the ports 5173, 8081 and 13714 are published on 127.0.0.1. In VS Code it installs clangd (reading `build/dev/compile_commands.json`) and CMake Tools with the presets.

### CLion

Use CLion's Docker toolchain (Settings > Build, Execution, Deployment > Toolchains > Docker) with the image `cpp-inertia-dev:latest` (built by `docker compose build`) and a container setting that mounts the project at `/src`. Select the `dev` CMake preset; it needs `VCPKG_ROOT=/opt/vcpkg`, which the image already sets. The `.clang-format` and `.clang-tidy` files are picked up automatically.

### Host tools (clangd, Serena, IDEs)

The container's `build/dev/compile_commands.json` uses container paths (`/src/...`), which host tools cannot open. Configure once, then convert it to a host-path database:

```sh
scripts/dev cmake --preset dev              # writes build/dev/compile_commands.json
python3 tools/host-compile-commands.py      # writes build/dev/host/compile_commands.json
```

The root `.clangd` points clangd at `build/dev/host`. Re-run the script after a reconfigure. It needs only python3 on the host, and it fails with the configure hint when the input is missing.

## Requirements

- **Using the library** needs a C++17 compiler: every installed header (core, ports, fake, Drogon and cpp-httplib adapters, conformance kit) and the conformance kit's sources compile as C++17. This is enforced by the `headers_cxx17` test and by the `package.*17` consumer tests, which build the installed package with `CMAKE_CXX_STANDARD=17`.
- **Building the library** (its own sources) needs a C++20 compiler. The minimum versions below are enforced when CMake configures (`cmake/Warnings.cmake`), and every warning flag is selected from them with generator expressions, so a flag that a compiler does not know is never passed:

  | Compiler | Minimum | Notes |
  | --- | --- | --- |
  | Clang (Linux) | 18 | `-Wfunction-effects` from Clang 20. **Tested: Clang 23.1.4 with libstdc++ 15** |
  | GCC | 13 | **Tested: GCC 15.2 with libstdc++ 15** (the `gcc` preset) |
  | Apple Clang (macOS) | 16 (Xcode 16, LLVM 17) | same flags as Clang, without `-Wfunction-effects`; **not tested yet** |
  | MSVC (Windows) | 19.40 (Visual Studio 2022 17.10) | `/W4 /WX /permissive- /utf-8 /Zc:__cplusplus`; **not tested yet** |

  The `flags_applied` test reads the compile database and fails when a library source loses one of the flags the table gives the compiler in use, or when a consumer of the library receives any of them.
- CMake 3.31 or later, and nlohmann_json. The adapters need Drogon or cpp-httplib, the conformance kit needs Catch2.

### Static and shared libraries

`BUILD_SHARED_LIBS=OFF` (the default) builds static libraries, `ON` builds shared ones (`libinertia_core.so.0.1`, `.dylib`, or a DLL with its import library). Both are installed and packaged the same way and consumed with the same `find_package(inertia)`.

- Shared libraries hide every symbol the public headers do not mark with `INERTIA_CORE_EXPORT`, `INERTIA_FAKE_EXPORT`, `INERTIA_DROGON_EXPORT` or `INERTIA_HTTPLIB_EXPORT` (generated by `generate_export_header` into `inertia/core_export.hpp`, `inertia/fake/export.hpp`, ...; the macros are empty in a static build). On Linux a version script also limits the dynamic symbols to namespace `inertia`: libstdc++ gives the `std::` and `nlohmann::` templates the library instantiates default visibility, and the `exports.*` tests check that none of them is exported.
- The soname is `libinertia_core.so.0.1`, that is `SOVERSION` = major.minor: while the major version is 0 a minor release may break the API and the ABI, as the package version file says (`SameMinorVersion`). It becomes the major version alone at 1.0.
- An installed shared library finds the others through `RPATH` (`$ORIGIN`, `@loader_path`). On Windows put the DLLs next to the program (CMake does it for the tests; the consumer in `tests/package` copies `$<TARGET_RUNTIME_DLLS>`).
- Shared libraries and their dependencies go together: use `x64-linux-dynamic`, `arm64-osx-dynamic` or `x64-windows` for vcpkg (the `*-shared` presets do). A static Drogon would be copied into `libinertia_drogon.so` and into the program, and the two copies of its `app()` singleton would not see each other. A shared inertia also needs the same C++ standard library and, on Windows, the dynamic C runtime (`/MD`) in the program and in the DLLs. On Windows a program built with another C runtime than the shared core (`/MT`, or `/MDd` against a release DLL) fails to link with `LNK2001` naming `inertia_core_requires_runtime_<runtime>`.
- C++17 consumers are unaffected: the export headers are plain C++17, and the `package.*17` tests build the installed static and shared packages as C++17.

Presets: `dev-shared` and `release-shared` (Linux, Docker), `gcc` (the same as `dev` with g++-15), `windows-msvc-{static,shared}` and `macos-{static,shared}-{debug,release}` (native, no Docker: `VCPKG_ROOT` must be set; the Windows ones use the Visual Studio 2022 generator and pick Debug or Release through the build and test presets `windows-msvc-<linkage>-<config>`). Debug libraries get a `d` suffix with MSVC. Windows and macOS have not been run yet: the presets and the portability code are written from the documentation and checked on Linux only.

## Using the library

### With `find_package`

Install it (`cmake --install build/dev --prefix <dir>`), then:

```cmake
find_package(inertia CONFIG REQUIRED COMPONENTS core drogon)   # components: core, fake, drogon, httplib
target_link_libraries(app PRIVATE inertia::drogon)             # brings inertia::core with it
# or, for cpp-httplib (find_package(httplib) is done for you):
# find_package(inertia CONFIG REQUIRED COMPONENTS core httplib)
# target_link_libraries(app PRIVATE inertia::httplib)
```

A package installed without an adapter (`-DINERTIA_BUILD_DROGON=OFF`, `-DINERTIA_BUILD_HTTPLIB=OFF`) refuses its component (`COMPONENTS drogon`, `COMPONENTS httplib`) with a clear message. Version compatibility is `SameMinorVersion` while the major version is 0.

### With `add_subdirectory` or `FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(inertia GIT_REPOSITORY <repository url> GIT_TAG <tag>)
FetchContent_MakeAvailable(inertia)
target_link_libraries(app PRIVATE inertia::drogon)
```

As a subproject, tests, examples, install rules and `-Werror` are off. Dependencies (nlohmann_json, and Drogon or cpp-httplib for an adapter) must be findable by `find_package`; the repository's `vcpkg.json` shows the set.

### A minimal Drogon application

```cpp
#include <inertia/drogon/drogon.hpp>
#include <inertia/inertia.hpp>
#include <inertia/session.hpp>

#include <drogon/HttpAppFramework.h>

#include <chrono>
#include <memory>

int main() {
    auto &app = drogon::app();
    app.enableSession(std::chrono::hours(1)); // required: flash data, errors and CSRF live there

    inertia::Config config;
    config.version = [] { return std::string{"1"}; }; // or inertia::versionFromVite(vite)
    auto inertiaApp = std::make_shared<inertia::Inertia>(
        std::move(config), std::make_shared<inertia::drogon::DrogonHttpClient>());
    inertiaApp->share("appName", "My app"); // shared props: register before install()

    app.registerHandler(
        "/users",
        [](const drogon::HttpRequestPtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            inertia::drogon::render(req, "Users/Index",
                                    {{"users", inertia::Json::array({"Ada", "Alan"})}},
                                    std::move(callback));
        },
        {drogon::Get});

    inertia::drogon::install(inertiaApp); // last: Inertia is immutable once installed
    app.addListener("0.0.0.0", 8080).run();
}
```

`install()` adds a pre-handling advice (asset version mismatch answers 409, CSRF check with the `XSRF-TOKEN` cookie / `X-XSRF-TOKEN` header, 419 when it fails) and a post-handling advice (`Vary: X-Inertia`, the CSRF cookie). `render()` answers exactly once: JSON for Inertia requests, the HTML page otherwise. Pass `CsrfOptions` to `install()` to rename the cookie/header, disable the check, exempt webhook paths or trust forwarded headers. `install()` can be called once (a second call throws `std::logic_error`); without `app().enableSession()` a CSRF-checked request (mutating, not exempt) and `render()` answer a 500 that says so, while other routes are untouched, and `DrogonSession` throws `std::invalid_argument` for a null session.

### A minimal cpp-httplib application

```cpp
#include <inertia/httplib/httplib.hpp>
#include <inertia/inertia.hpp>
#include <inertia/session.hpp>

#include <memory>

int main() {
    inertia::Config config;
    config.version = [] { return std::string{"1"}; };
    auto inertiaApp = std::make_shared<inertia::Inertia>(
        std::move(config), std::make_shared<inertia::httplib::HttplibHttpClient>());
    inertiaApp->share("appName", "My app"); // shared props: register before install()

    httplib::Server server;
    inertia::httplib::install(server, inertiaApp); // before listen(): Inertia is immutable once installed

    server.Get("/users", [](const httplib::Request &req, httplib::Response &res) {
        inertia::httplib::render(req, res, "Users/Index",
                                 {{"users", inertia::Json::array({"Ada", "Alan"})}});
    });
    server.Post("/users", [](const httplib::Request &, httplib::Response &res) {
        auto session = inertia::httplib::session(res);              // this request's session
        inertia::flash(session, "success", "Saved.");
        inertia::httplib::toHttplib(inertia::redirect("/users"), res);
    });
    server.listen("0.0.0.0", 8080);
}
```

Same contract as the Drogon adapter, adapted to cpp-httplib's synchronous handlers:

- `install(server, inertia, Options{})` sets the **pre-routing** handler (asset version mismatch answers 409; CSRF check with the `XSRF-TOKEN` cookie / `X-XSRF-TOKEN` header on every method but GET/HEAD/OPTIONS, constant-time compared, 419 when it fails) and the **post-routing** handler (`Vary: X-Inertia`, the `XSRF-TOKEN` cookie, the session cookie). cpp-httplib runs the post-routing handler for every response, including those the pre-routing handler short-circuited, 404s and 500s. A second `install()` on the same server throws. **Never call `set_pre_routing_handler` / `set_post_routing_handler` yourself after `install()`**: cpp-httplib has one slot of each, so that would silently switch off the version check, CSRF, session cookies and `Vary` handling. Pass your own logic as `Options::preRouting` (runs after Inertia's step, only when Inertia did not answer; return `Handled` to answer yourself) and `Options::postRouting` (runs before Inertia's step, so `finalize` always sees the final response). `Options` also carries the `CsrfOptions` (rename cookie/header, disable, exempt webhook paths), `trustForwardedHeaders`, the session store and an error logger.
- `render(req, res, component, props)` fills `res` before it returns (JSON for Inertia requests, the HTML page otherwise). A prop that throws is logged and answered with a 500; the server keeps serving. `location(req, res, url)` writes the external redirect (409 + `X-Inertia-Location`, or 302). `session(res)` is the request's `SessionStore`.
- `HttplibRequest{req}` (`RequestView`) takes the URL from the raw request target, so `page.url` is exactly what the client sent. Pass `trustForwardedHeaders = true` in `Options` only behind a reverse proxy that sets or strips `X-Forwarded-Proto` / `X-Forwarded-Host`: `fullUrl()` then uses them (first value of the list) and the cookies become `Secure` for https requests. Off by default, because any client can forge them.
- **Sessions**: cpp-httplib has none, so `MemorySessionStore` keeps them: a 256-bit random id from the operating system's CSPRNG (`getrandom(2)` on Linux, `BCryptGenRandom` on Windows, `arc4random_buf` on macOS; the server refuses to answer if there is none, never falls back), sent in the `inertia_session` cookie (`HttpOnly; SameSite=Lax; Path=/`, plus `Secure` for https). Ids a client sends that are unknown or expired are never adopted. A session is forgotten after an idle TTL (30 minutes by default, `MemorySessionOptions::idleTtl`). At most `MemorySessionOptions::maxSessions` (100 000) sessions are kept; at the cap the least recently used one is evicted, and expired ones are dropped, oldest first, whenever a session is opened, so anonymous floods cannot grow memory without bound. Sessions are **lazy**: a request whose handler never calls `session(res)` / `render()` and that needs no CSRF check (a 404, a static file, a health check) gets no session and no cookie. Pass your own store through `Options::sessions` to tune it or to inspect it.
- **Session fixation**: call `inertia::httplib::regenerateSession(res)` right after a successful login (and on any privilege change). The session keeps its data under a new random id, the old id stops working, the CSRF token is renewed and the new cookies are sent.
- **Secure cookies without TLS**: the server itself speaks plain HTTP, so the session and `XSRF-TOKEN` cookies are `Secure` only for requests whose trusted `X-Forwarded-Proto` is `https` (`trustForwardedHeaders`); terminate TLS in a proxy that sets it. `HttplibRequest` also reduces an absolute-form target (`GET http://host/x`) to `/x`, keeps `OPTIONS *` as `*`, and ignores a `Host` header that cannot be a URL authority.
- `exceptPaths` of the CSRF options are matched against the decoded request path as is: they are not normalized (no `..` or duplicate-slash folding), so list the exact paths your routes use.

### Prop types cheat-sheet

`Props` is a list of `{name, Prop}`. A `Prop` converts implicitly from any JSON value or from a closure (`[] { return inertia::Prop{...}; }`) that is resolved lazily, only when the prop is included in the response.

| You write | Behaviour |
| --- | --- |
| `{"a", 1}`, `{"a", Json{...}}` | A plain value, always sent on full visits and when requested by a partial reload. |
| `{"a", [] { return Prop{...}; }}` | A closure, resolved only when the prop is included. |
| `inertia::optional(fn)` | Only resolved when a partial reload asks for it by name. |
| `inertia::always(prop)` | Included even in partial reloads that did not ask for it. |
| `inertia::defer(fn, "group")` | Left out of the first response and listed in `deferredProps`; the client fetches each group in a follow-up request. |
| `defer(...).rescue()` | If the resolver throws, the page still succeeds and the prop is listed in `rescuedProps`. |
| `defer(...).merge()` | A deferred prop that merges on later reloads. |
| `inertia::merge(prop)` / `.merge()` | Append new values to the client's list (`mergeProps`). |
| `.prepend()`, `inertia::deepMerge(prop)` / `.deepMerge()` | Prepend instead of append; deep merge of nested objects. |
| `.append("data")`, `.prepend("data")`, `.append("data", "id")` | Merge only at a sub path, optionally matching items by key (`matchPropsOn`). |
| `.matchOn("id")` | Replace items having the same key instead of duplicating them. |
| `inertia::once(fn)` | Resolved on the first visit, then remembered by the client. Modifiers: `.as("key")` shares the cache key, `.until(std::chrono::minutes{5})` expires it, `.fresh()` forces a refresh. `Inertia::shareOnce(key, fn)` shares one. |
| `inertia::scroll(fn, metadata, "data")` | Infinite scroll: the data wrapper plus `ScrollMetadata` (`pageName`, `previousPage`, `nextPage`, `currentPage`) in `scrollProps`. Chain `.deferred()` to defer it. |
| `inertia::object({{"k", Prop}, ...})` | A nested object whose members may themselves be prop types (partial reloads address them as `parent.child`). |

```cpp
inertia::Props{
    {"user", user},                                                       // plain
    {"stats", inertia::defer([] { return inertia::Prop{loadStats()}; }, "stats")},
    {"errorsCount", inertia::always(count)},
    {"posts", inertia::merge(page).append("data", "id")},
    {"plans", inertia::once([] { return inertia::Prop{loadPlans()}; }).until(std::chrono::hours{1})},
};
```

Shared props: `Inertia::share(key, prop)` for constants and `Inertia::shareWith(fn)` for per-request values (`fn(const RequestView &, SessionStore &)`). `Config` also offers `rootId`, `encryptHistory`, `exposeSharedPropKeys`, `preserveBigIntegers`, `withAllErrors` and a `rootTemplate` for the first-visit HTML.

### Session helpers and responses

These take a `SessionStore` (use `inertia::drogon::DrogonSession{req->session()}`, or `inertia::httplib::session(res)` with cpp-httplib) and are consumed by the next page render.

```cpp
inertia::drogon::DrogonSession session{req->session()};

inertia::withErrors(session, {{"email", {"The email is required."}}}); // bag "default"
inertia::withErrors(session, errors, "login");                         // a named error bag
inertia::flash(session, "success", "Saved.");                          // one-shot flash data
inertia::clearHistory(session);                                        // clearHistory, once
inertia::preserveFragment(session);                                    // preserveFragment, once

auto res = inertia::redirect("/users");                                // 302, Inertia turns PUT/PATCH/DELETE into 303
auto prev = inertia::back(inertia::drogon::DrogonRequest{req});        // 302 to the Referer, or "/"
auto ext = inertia::drogon::location(req, "https://example.com");      // 409 + X-Inertia-Location, or 302
callback(inertia::drogon::toDrogon(res));
```

### Vite and SSR

```cpp
auto vite = std::make_shared<const inertia::Vite>(
    inertia::ViteConfig{.publicDir = "public", .buildDir = "build", .hotFile = "hot",
                        .entries = {"src/app.tsx"}},
    std::make_shared<inertia::DiskFileReader>()); // reads the files; <inertia/disk_file_reader.hpp>
config.version = inertia::versionFromVite(vite); // hash of the manifest: a new build triggers a 409 reload
config.vite = vite;                              // first-visit HTML gets the script/link tags
```

`DiskFileReader` (std::filesystem only) stamps the manifest with its size and nanosecond modification time, so the manifest is read, parsed and hashed once and again only after a new build: an unchanged manifest costs one `stat` per request, and the asset version is computed once per manifest content. A stamp cannot see a same-size edit made within one clock tick of the previous one; that does not happen for a build output. To read files another way, implement `inertia::FileReader` (`read`, and `stamp` if you can make it cheap).

While `public/hot` exists (the Vite dev server writes it, see `examples/react-app/frontend/vite.config.ts`) the dev-server tags are emitted; otherwise the tags come from `public/build/.vite/manifest.json`. For SSR build the server bundle (`vite build --ssr src/ssr.tsx`), run it (`bun dist/ssr/ssr.js`, port 13714) and enable it:

```cpp
config.ssr.enabled = true;            // url "http://127.0.0.1:13714", timeout 1500 ms, exceptPaths
// pass inertia::drogon::DrogonHttpClient (or inertia::httplib::HttplibHttpClient) as the second
// argument of the Inertia constructor
```

If the SSR server fails or times out, the page falls back to client-side rendering.

## Packaging

```sh
scripts/dev sh -c 'cmake --workflow --preset package'
```

The `package` workflow configures the `release` preset (static libraries), builds it, runs its tests, then runs CPack twice (the `tgz` and `deb` package presets). `package-shared` does the same for `release-shared`. The packages land in `build/release` and `build/release-shared`; the linkage is in every name so that both can be released side by side:

- `inertia-<version>-<system>-<arch>-<static|shared>.tar.gz` (`.zip` on Windows): one archive with every built component, under a top directory of the same name. Unpack it anywhere and point `CMAKE_PREFIX_PATH` at that directory.
- one `.deb` per component (`libinertia-<component>-dev_<version>-1_<arch>.deb`, section `libdevel`). Every package but the core one depends on exactly the same version of `libinertia-core-dev`, and the core one on `nlohmann-json3-dev`. A shared build also gets one runtime package per library (`libinertia-core0.1`, ..., section `libs`) holding the versioned shared object; the development package depends on it. Static builds have no shared-library dependencies. Drogon, cpp-httplib and Catch2 have no stable Debian package names, so the adapter and conformance packages do not declare them: provide them yourself (vcpkg or a source build). The files install under `/usr`, and `find_package(inertia)` finds them there.

| Component (CPack) | Debian package | Contents |
| --- | --- | --- |
| `inertia_core` | `libinertia-core-dev` | headers, `libinertia_core.a`, the CMake package files, `LICENSE` and `README.md` under `share/doc/inertia` |
| `inertia_fake` | `libinertia-fake-dev` | the in-memory fake adapter |
| `inertia_drogon` | `libinertia-drogon-dev` | the Drogon adapter (built unless `INERTIA_BUILD_DROGON=OFF`) |
| `inertia_httplib` | `libinertia-httplib-dev` | the cpp-httplib adapter (built unless `INERTIA_BUILD_HTTPLIB=OFF`) |
| `inertia_conformance` | `libinertia-conformance-dev` | the conformance kit: headers, scenario sources and the `inertia_add_conformance_test` helper |

Only the components of the configured build are packaged, so a core-only build yields the core and fake packages. Set `SOURCE_DATE_EPOCH` before running CPack to get packages with stable timestamps. The `package.cpack.*` tests (label `package`) build a consumer against the extracted TGZ, in C++17 and C++20, and check the control fields of the `.deb` files with `dpkg-deb`; the `.deb` files are never installed.

### Personal presets

`CMakeUserPresets.json` is gitignored: use it for settings that belong to your machine. For example, a preset that inherits `dev`, runs include-what-you-use and uses another vcpkg checkout:

```json
{
  "version": 10,
  "configurePresets": [
    {
      "name": "dev-iwyu",
      "inherits": "dev",
      "binaryDir": "${sourceDir}/build/dev-iwyu",
      "toolchainFile": "/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "INERTIA_IWYU": "ON"
      }
    }
  ],
  "buildPresets": [
    { "name": "dev-iwyu", "configurePreset": "dev-iwyu" }
  ]
}
```

## Architecture

Ports and adapters.

- The **core** is pure and synchronous (SSR goes through a callback). It talks to the world through ports: `RequestView`, `Response` (a plain value), `SessionStore`, `HttpClient` (async, for SSR) and `FileReader` (Vite manifest, hot file). A CTest check fails if `drogon` or `httplib` ever appears in `include/inertia/` or `src/`.
- **Adapters** implement the ports for a framework: `adapters/drogon` and `adapters/httplib` are the real ones, `adapters/fake` is in-memory.
- A **conformance kit** (`conformance/`, installable as the component `conformance`) runs the same HTTP-level protocol scenarios against every adapter.

To write a new adapter for another framework, follow [docs/writing-an-adapter.md](docs/writing-an-adapter.md): the ports to implement, the request pipeline, and how to prove the adapter with the conformance kit (`find_package(inertia COMPONENTS core conformance)` and `inertia_add_conformance_test()`). `adapters/httplib` is the simplest reference. Props-building code stays portable.

## Protocol coverage

- `docs/protocol-coverage.toml` holds one entry per protocol rule (stable id, source, status `done` / `todo` / `na` with a reason, and `e2e = true` when a client can observe it).
- Catch2 tests carry `[protocol:<id>]` tags and Playwright tests carry the same tags in their titles.
- The `coverage_check` CTest (`tools/coverage_check.py`) fails when a `done` rule has no test, a tag is unknown, or the matrix is malformed. With `--strict` (the `release` preset) it also fails on any `todo` rule and on any `e2e` rule without a Playwright test (`--e2e-list` takes the output of `bunx playwright test --list`).
- `tools/check-inertia-version.py` compares the newest `@inertiajs/react` on npm with the pinned version (exit 1: a new release should be reviewed against the matrix).
- The table below is generated: `python3 tools/coverage_check.py --matrix docs/protocol-coverage.toml --markdown`. A unit test fails when it is stale.

<!-- coverage:start -->

| Rule | Behavior | Status | Browser-tested |
| --- | --- | --- | --- |
| `html.script-element` | First visit returns 200 text/html with a data-page JSON script element and a root div | done | yes |
| `html.json-escaping` | Page JSON in the script element escapes slash, less-than and greater-than without HTML entities | done | yes |
| `html.root-id` | Root id is configurable (default app) and data-page equals that id | done | yes |
| `json.response` | Inertia request returns 200 JSON with X-Inertia true and application/json content type | done | yes |
| `vary.always` | Every response carries Vary: X-Inertia, appended to an existing value and deduplicated | done | yes |
| `page.required` | Page object always has component, props (including errors), url and version | done | no |
| `page.url.trailing-slash` | url is the request path and query as received, with any trailing slash preserved | done | no |
| `page.version.empty` | version is the empty string when assets are not versioned | done | no |
| `page.encrypt-history` | encryptHistory flag is emitted only when enabled globally or by a per-response override | done | yes |
| `page.clear-history` | clearHistory flag is emitted once, pulled from the inertia.clear_history session key | done | yes |
| `page.preserve-fragment` | preserveFragment flag is emitted once, pulled from the inertia.preserve_fragment session key | done | yes |
| `page.flash` | flash object is pulled from the session and omitted when empty; repeated flash calls merge | done | yes |
| `page.shared-props` | sharedProps lists top-level shared keys, using the first segment of dot keys | done | yes |
| `page.bigint` | Integers outside the JS safe range serialize as a bigint object when preserveBigIntegers is set | done | yes |
| `page.metadata.omit-empty` | Metadata fields are emitted only when non-empty | done | no |
| `version.mismatch` | GET Inertia request with a differing X-Inertia-Version gets 409 with X-Inertia-Location and X-Inertia-Version | done | yes |
| `version.non-get` | Non-GET methods never receive the version-mismatch 409 | done | no |
| `redirect.303` | Inertia 302 becomes 303 for PUT, PATCH and DELETE, but not for POST | done | yes |
| `redirect.fragment` | Inertia non-prefetch redirect whose Location contains a hash becomes 409 with X-Inertia-Redirect | done | yes |
| `redirect.external` | location(url) gives 409 with X-Inertia-Location for Inertia requests and 302 with Location otherwise | done | yes |
| `response.empty` | Inertia 200 with an empty body becomes 302 to Referer or to /, and a body of 0 is not empty | done | yes |
| `partial.component-match` | Request is partial only when X-Inertia-Partial-Component equals the rendered component | done | yes |
| `partial.only` | only paths include the matching path, its descendants and its ancestors | done | yes |
| `partial.except` | except paths exclude the matching path and its descendants, and win over only | done | yes |
| `partial.dot-notation` | Prop paths are dot-paths; top-level dot keys unpack into nested objects, nested dot keys do not | done | yes |
| `partial.resolved-children` | Children of a value produced by resolving a closure bypass the partial filters | done | yes |
| `props.dot-keys` | Top-level dot keys are unpacked into nested objects before resolution | done | no |
| `always.partial` | Always props are resolved on full and partial visits and bypass both filters | done | yes |
| `optional.full` | Optional props are skipped with no metadata on a full visit | done | yes |
| `optional.partial` | Optional props are resolved on partial visits only when selected | done | yes |
| `defer.groups` | Deferred props on a full visit are listed in deferredProps by group, default group default | done | yes |
| `defer.partial` | Deferred props resolve when selected on partial visits, and deferredProps is never emitted on partials | done | yes |
| `defer.rescue` | Deferred resolver that throws with rescue enabled is listed in rescuedProps and omitted from props | done | yes |
| `defer.merge` | Deferred merge props emit merge metadata even while still deferred | done | yes |
| `merge.append` | Root append props are listed in mergeProps | done | yes |
| `merge.prepend` | Root prepend props are listed in prependProps | done | yes |
| `merge.deep` | Deep merge props are listed in deepMergeProps | done | yes |
| `merge.nested-paths` | Nested append or prepend paths emit path.sub entries in mergeProps or prependProps | done | yes |
| `merge.match-on` | A matchOn key adds propPath.key to matchPropsOn, extended to propPath.sub.key for nested paths | done | yes |
| `merge.reset` | Merge metadata is suppressed for paths listed in X-Inertia-Reset | done | yes |
| `merge.partial-filter` | Partial reloads emit merge metadata only for only paths and their descendants, not excluded by except | done | yes |
| `once.metadata` | onceProps entry carries prop and expiresAt, the latter from ttl in milliseconds or null | done | yes |
| `once.except-header` | Once prop is dropped from props on non-partial, non-fresh Inertia requests whose key is in X-Inertia-Except-Once-Props, but its onceProps entry stays | done | yes |
| `once.partial-ignores-header` | X-Inertia-Except-Once-Props is ignored on partial reloads | done | yes |
| `once.fresh` | A fresh once prop is resolved even when its key is listed in X-Inertia-Except-Once-Props | done | yes |
| `once.custom-key` | Once custom key, not the prop path, is matched against X-Inertia-Except-Once-Props | done | yes |
| `once.defer` | A deferred once prop already loaded has no deferredProps entry but keeps its onceProps entry | done | yes |
| `scroll.metadata` | scrollProps carries pageName, previousPage, nextPage, currentPage and reset for each scroll path | done | yes |
| `scroll.merge-intent` | Scroll merge label goes to prependProps when the intent header is prepend, otherwise to mergeProps | done | yes |
| `scroll.reset` | scrollProps reset true for paths in X-Inertia-Reset and suppresses the merge label | done | yes |
| `scroll.deferred` | Deferred scroll props emit deferredProps and merge metadata but no scrollProps | done | yes |
| `errors.empty` | errors is an always prop and is an empty object, never an array, when there are no errors | done | yes |
| `errors.first-message` | Each field maps to its first message, or to a string array in all-errors mode | done | yes |
| `errors.bag` | With X-Inertia-Error-Bag, errors are scoped as bag then field | done | yes |
| `ssr.render` | SSR POSTs the page JSON to ssrUrl /render and substitutes head and body into the response | done | yes |
| `ssr.fallback` | Any SSR failure (connection, non-2xx, invalid JSON) falls back to client-side rendering | done | no |
| `ssr.health` | GET /health returns a status OK object | done | no |
| `asset.version-default` | Default asset version is a hash of the Vite manifest file that changes with the assets | done | no |
| `laravel.previous-url` | Laravel-only: Laravel previous-URL storage | n/a: Laravel previous-URL storage; C++ has no session-backed previous URL concept | no |
| `laravel.precognition` | Laravel-only: Laravel Precognition live validation is not part of the server protocol | n/a: Laravel Precognition live validation is not part of the server protocol | no |
| `laravel.exception-handler` | Laravel-only: Laravel exception handler integration | n/a: Laravel exception handler integration; there is no framework exception handler | no |
| `laravel.ensure-pages-exist` | Laravel-only: Laravel ensure_pages_exist development check | n/a: Laravel ensure_pages_exist development check; C++ components are not discovered from pages on disk | no |
| `laravel.mix-manifest` | Laravel-only: Laravel Mix manifest | n/a: Laravel Mix manifest; the asset version uses the Vite manifest instead | no |
| `laravel.devtools-recorder` | Laravel-only: Laravel devtools recorder | n/a: Laravel devtools recorder; not part of the C++ library | no |
| `laravel.php-enums-arrayable` | Laravel-only: PHP enums and Arrayable/Responsable objects | n/a: PHP enums and Arrayable/Responsable objects; C++ has no equivalent, props are plain values | no |
| `laravel.provides-inertia-properties` | Laravel-only: ProvidesInertiaProperties | n/a: ProvidesInertiaProperties; the C++ equivalent is plain functions returning Props | no |

<!-- coverage:end -->

## Quality gates

Every preset has configure, build, test and workflow presets of the same name, so one command does it all:

| Command | What it proves |
| --- | --- |
| `scripts/dev cmake --workflow --preset dev` | Debug build with `-Werror` and every test (unit, conformance on fake, Drogon and cpp-httplib, Drogon and cpp-httplib adapters, `coverage_check`, `core_has_no_adapter_frameworks`, headers self-contained, package consumer tests). |
| `scripts/dev cmake --workflow --preset core-only` | The core builds and passes without Drogon or cpp-httplib installed. |
| `scripts/dev cmake --workflow --preset asan` | AddressSanitizer + UBSan: the `undefined` group plus `integer` (signed overflow, shifts, implicit conversions), `nullability`, `float-divide-by-zero` and `local-bounds`; any report fails the test. `unsigned-integer-overflow` is excluded because libstdc++ `std::string::compare` wraps on purpose. TypeSanitizer was tried and is not kept: its runtime reports false positives inside libstdc++ `std::tuple`. |
| `scripts/dev-tsan cmake --workflow --preset tsan` | ThreadSanitizer, with instrumented dependencies (`x64-linux-tsan` triplet). |
| `scripts/dev cmake --workflow --preset release` | Release build with strict coverage: no `todo` rule left. |
| `scripts/dev sh -c 'cd e2e && bunx playwright test'` | The official client against the example app (production build). Set `INERTIA_SSR=1` to run again with server-side rendering, and `INERTIA_E2E_SERVER=httplib` (default `drogon`) to run it on the cpp-httplib adapter instead of Drogon; the suite is the same and must pass on both. Build both example servers and `bun run build` in `examples/react-app/frontend` first. |
| `python3 -m unittest discover -s tools/tests` | The Python tools (`coverage_check.py`, `check-inertia-version.py`, README table). Also run by CTest as `tools_unit_tests`. |

Run `pre-commit install` once after cloning: it installs both the pre-commit and the commit-msg hook.

`pre-commit run --all-files` runs the commit hooks:

- **clang-format** (v23.1.3, the ready-made `pre-commit/mirrors-clang-format` hook) formats the C/C++ files. It runs on the host: no build, no Docker.
- **clang-tidy** (v23.1.3 wheel, the ready-made `cpp-linter/cpp-linter-hooks` hook) runs on the host on `.cpp` files, with the exact flags of each file from `build/dev/host/compile_commands.json`. That database is the container one with its `/src` paths rewritten to host paths. Generate it once per clone, and again after adding or removing a source file:
  ```sh
  scripts/dev cmake --preset dev -DINERTIA_BUILD_EXAMPLES=ON   # configure only, no build
  python3 tools/host-compile-commands.py
  ```
  Without the database the hook fails and says so; it never skips silently. The hook is configured with `--all-lines` (every diagnostic of a staged file, not only those on changed lines). To lint the whole tree (every tracked `.cpp` except the out-of-tree `tests/package`), run `pre-commit run clang-tidy --all-files`; it takes a few minutes and must report nothing. `.clang-tidy` limits diagnostics to the project's own headers (`include/inertia`, `src`, `adapters`, `tests`, `examples`), so vcpkg and system headers stay quiet. `ExcludeHeaderFilterRegex` keeps the build trees (`build`, `vcpkg_installed`, `.cache`) out, wherever the clone lives.
- **hadolint** (v2.15.1, the ready-made `hadolint-docker` hook, image pinned to the same tag) lints `docker/Dockerfile`. It runs in Docker.
- **dclint** (`zavoloklom/dclint:3.1.0`, a local hook because the upstream project ships no pre-commit hook) lints `compose.yaml`. It runs in Docker.
- **include-what-you-use** is not a pre-commit hook: it is the CMake option `INERTIA_IWYU` (OFF by default). When ON, the compilation of the project's own targets (libraries, tests, conformance executables, example) runs include-what-you-use 0.27 (same LLVM major as clang, pinned in `mise.toml`) and fails on any suggestion; dependencies and embedding projects are never touched, because the command is set per target by `inertia_enable_iwyu()` (`cmake/InertiaIwyu.cmake`), not through `CMAKE_CXX_INCLUDE_WHAT_YOU_USE`. The `lint` CI job enforces it. To enable it locally, put this in a `CMakeUserPresets.json` (not tracked), configure with `scripts/dev cmake --preset dev-iwyu` and build `build/dev` as usual (`scripts/dev cmake --build build/dev`):
  ```json
  {
    "version": 10,
    "configurePresets": [
      { "name": "dev-iwyu", "inherits": "dev", "cacheVariables": { "INERTIA_IWYU": "ON" } }
    ]
  }
  ```
  Third-party quirks (nlohmann implementation headers, Catch2, Drogon, libstdc++ range algorithms) and the `<inertia/...>` spelling of the project's own headers are mapped in `.iwyu.imp`; the sources carry no `keep`, `no_include` or `no_forward_declare` pragma. The only pragma is `IWYU pragma: export` in `inertia/json.hpp`, which deliberately exposes nlohmann/json. IWYU only reports on a `.cpp` and its own header, so a header-only header is checked through the sources that include it.
- The tree contains no inline clang-tidy suppression comment: a finding is fixed in the code, not silenced.
- A Conventional Commits check on the commit message (`feat(core): ...`, `fix(drogon): ...`).

## Coverage

Line coverage uses clang source-based instrumentation (`-fprofile-instr-generate -fcoverage-mapping`):

```sh
scripts/dev cmake --workflow --preset coverage   # configure, build, test; profiles land in build/coverage/profiles
scripts/dev tools/coverage-report.sh              # merge, print the llvm-cov summary, write the HTML report
```

The report is `build/coverage/html/index.html`. It covers `include/inertia`, `src` and `adapters/*`; vcpkg, system headers, tests and build output are ignored. The script fails when the total line coverage drops below `INERTIA_COVERAGE_MIN_LINES` (default set at the top of `tools/coverage-report.sh`, the single place to raise it; override per run with `scripts/dev env INERTIA_COVERAGE_MIN_LINES=99 tools/coverage-report.sh`). `llvm-cov` and `llvm-profdata` come from the `conda:llvm-tools` mise pin, at the same version as clang.

## API documentation

The API reference is built with Doxygen from the public headers (`include/inertia`, excluding `detail`, and the adapter headers). It is off by default and not part of `all`:

```sh
scripts/dev cmake --preset dev -DINERTIA_BUILD_DOCS=ON   # configure with the docs target
scripts/dev cmake --build --preset dev --target docs     # writes build/dev/docs/html/index.html
```

Every documented entity must be complete: Doxygen warnings (undocumented members, missing `@param`) fail the `docs` target.

## Compiler cache

Every preset compiles through [sccache](https://github.com/mozilla/sccache) (`CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER` in the base configure preset, sccache pinned in `mise.toml`). Locally the cache lives in the `sccache-cache` Docker volume (`SCCACHE_DIR`, 10G): a rebuild of a wiped `build/dev` is served from it. Print the hit rate with `scripts/dev sh -c 'cmake --build --preset dev && sccache --show-stats'` (the stats belong to the container's own server). In CI the GitHub Actions cache backend is used (`SCCACHE_GHA_ENABLED`). To reset the local cache: `docker volume rm cpp-inertia_sccache-cache`.

## Continuous integration

`.github/workflows/ci.yml` runs on pushes to `main` and `feat/**` and on pull requests; a newer run on the same ref cancels the older one, and no matrix cell cancels its siblings (`fail-fast: false`). Third-party actions are pinned to a full commit SHA, tools to a version.

| Job | Runner | What it runs |
| --- | --- | --- |
| `linux` | ubuntu-26.04, Docker dev image | matrix `preset: [dev, dev-shared, gcc, asan, core-only, package, package-shared]`: `cmake --workflow --preset <preset>` through `scripts/dev` (`package*` also build and test the TGZ and `.deb`) |
| `e2e` | ubuntu-26.04, Docker | matrix `server: [drogon, httplib]`: builds the example servers and the front end, then runs the Playwright suite against that adapter, in client mode and with `INERTIA_SSR=1` |
| `coverage` | ubuntu-26.04, Docker | `cmake --workflow --preset coverage`, then `tools/coverage-report.sh` (fails under 96% lines). The HTML report is the `coverage-html` artifact. |
| `lint` | ubuntu-26.04, Docker | the only job with static analysis: IWYU build, Doxygen (warnings are errors), clang-tidy on the whole tree and the other pre-commit hooks (below) |
| `macos` | macos-26 (arm64), Apple Clang, vcpkg | matrix of the workflow presets `macos-static-debug`, `macos-shared-debug`, `package-macos-static`, `package-macos-shared` (the last two run the release preset, then the TGZ). Prints the Apple Clang version and whether it accepts `-Wfunction-effects` (not enabled yet). |
| `windows` | windows-2022, MSVC 2022, vcpkg | matrix of `windows-msvc-static-debug`, `windows-msvc-shared-debug`, `package-windows-msvc-static`, `package-windows-msvc-shared` (Release, then the ZIP). Shared cells run `ctest -R package.runtime_mismatch`; every cell runs `tools/check-runtime-guard.sh` (`dumpbin`: the core DLL exports exactly one `inertia_core_requires_runtime_*`, MD for Release and MDd for Debug; a static build has none). |
| `windows-vs2019-consumer` | windows-2022 | installs the shared ZIP built above and builds a C++17 consumer with the v142 toolset (cl 19.29, VS 2019 16.11), from the VS 2022 component, since windows-2019 images are retired. Shared only: an older linker cannot link static libraries built by a newer toolset. |
| `commits` | ubuntu-26.04 | every commit message of the push or pull request range against Conventional Commits |

macOS and Windows only build and test, with warnings as errors (`-Werror`, `/W4 /WX`).

Caches: the Docker jobs persist the vcpkg binary cache (`actions/cache`) and use sccache's GitHub backend; macOS does the same (vcpkg at the manifest baseline, files backend). Windows persists the vcpkg binary cache only: the Visual Studio generator ignores compiler launchers, so sccache would need the Ninja generator and `/Z7` debug info instead of the presets' `/Zi`.

The `lint` job runs clang-tidy on the whole tree through the pre-commit hook, behind [ctcache](https://github.com/matus-chochlik/ctcache) (pinned): `tools/lint-clang-tidy.sh` (run in the dev container) puts `clang-tidy-cache` in front of the hook's clang-tidy. A file is skipped when its preprocessed text (so any header it includes), flags and `.clang-tidy` configuration match a passing run; the cache directory is restored and saved with `actions/cache`. Diagnostics become annotations through the problem matchers in `.github/problem-matchers/`, and the job summary shows the diagnostics count and the ctcache hits and misses. Locally: `scripts/dev tools/lint-clang-tidy.sh` after configuring `build/dev`.

## Known limitations

- Drogon's `HttpClient` has no cancel API and does not pipeline, so a timed-out SSR request keeps its connection busy until the server answers. `DrogonHttpClient` therefore drops the origin's pooled client after a timeout (at most once per timeout window), and the next render opens a fresh connection instead of queueing behind the hung one. The abandoned request itself still runs to completion in the background, and a persistently hung SSR server costs one new connection per timeout window. Keep `SsrConfig::timeout` short; the page falls back to client-side rendering when it expires.
- Behind a TLS-terminating proxy, `DrogonRequest::fullUrl()` (the `X-Inertia-Location` of the 409 sent on an asset version mismatch) and the `Secure` flag of the CSRF cookie follow the connection, so they say `http` by default. Set `CsrfOptions::trustForwardedHeaders = true` to use the first value of `X-Forwarded-Proto` / `X-Forwarded-Host` instead. Only do that when the proxy always sets those headers: a client that reaches the server directly can forge them. The flag is applied to the requests the adapter builds itself (`install()`, `render()`, `location()`); a `DrogonRequest` you construct takes it as a second constructor argument.
- `toDrogon` turns each `Set-Cookie` into a Drogon cookie, and Drogon keys cookies by name: two cookies with the same name but a different `Path` or `Domain` collapse into one.
- Drogon and cpp-httplib are the only real adapters. The ports make another framework a small piece of work (see Architecture).
- cpp-httplib adapter: sessions are **in memory and single-process**. They are lost on restart and not shared between processes or hosts (use sticky sessions behind a proxy, or the Drogon adapter, for that). Memory is bounded by the idle TTL and the session cap (least recently used evicted first).
- cpp-httplib adapter: the SSR client is **blocking**. `render()` waits for the SSR server (up to `SsrConfig::timeout`) on the connection's thread, and cpp-httplib serves one connection per thread, so a slow SSR server ties up worker threads. Keep the timeout short and size the thread pool (`Server::new_task_queue`) for it. Only `http://` SSR URLs work: the adapter is built without TLS. `HttplibHttpClient` never throws, but it does not catch an exception thrown by the `done` callback you pass it.
- cpp-httplib adapter: `install()` takes over the server's pre-routing and post-routing handlers, and the handlers must be registered on the same server before `listen()`.
- Laravel-specific features (Precognition, previous-URL storage, exception handler) are out of scope: see the `na` rows above.
- The official Inertia Playwright suite is not reused: its server is a hand-written Express app that builds every page object literally (349 routes), so porting it would test those fixtures rather than this library.
