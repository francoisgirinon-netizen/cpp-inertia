# cpp-inertia

A C++20 implementation of the server side of the [Inertia.js](https://inertiajs.com) v3 protocol.

- **Framework-agnostic core** (`inertia::core`): pure C++ that depends only on nlohmann_json. It builds the page object, resolves props (partial reloads, deferred, merge, once, scroll, ...), renders the first-visit HTML and talks to an SSR server.
- **Drogon adapter** (`inertia::drogon`): one `install()` call and a `render()` helper.
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
- `scripts/dev-serve <cmd...>` is the same with the ports published (8080 Drogon, 5173 Vite, 13714 SSR). Use it for anything that serves.
- `scripts/dev-tsan <cmd...>` uses a service with a relaxed seccomp profile, needed only by ThreadSanitizer: `scripts/dev-tsan cmake --workflow --preset tsan`.

Run the example application:

```sh
# production mode: build the server and the front end, then serve
scripts/dev sh -c 'cmake --preset dev -DINERTIA_BUILD_EXAMPLES=ON && cmake --build --preset dev \
  && cd examples/react-app/frontend && bun install && bun run build'
scripts/dev-serve build/dev/examples/react-app/server/inertia_example      # http://localhost:8080

# development mode with hot reload: Vite writes public/hot, the server then emits dev tags
scripts/dev-serve sh -c 'cd examples/react-app/frontend && bun run dev'    # in one terminal
scripts/dev-serve build/dev/examples/react-app/server/inertia_example      # in another

# server-side rendering: the SSR server (:13714) must share the container's localhost with the
# example, so start both from one command (INERTIA_SSR is forwarded from the host)
INERTIA_SSR=1 scripts/dev-serve sh -c '(cd examples/react-app/frontend && bun run ssr) & \
  exec build/dev/examples/react-app/server/inertia_example'
```

### CLion

Use CLion's Docker toolchain (Settings > Build, Execution, Deployment > Toolchains > Docker) with the image `cpp-inertia-dev:latest` (built by `docker compose build`) and a container setting that mounts the project at `/src`. Select the `dev` CMake preset; it needs `VCPKG_ROOT=/opt/vcpkg`, which the image already sets. The `.clang-format` and `.clang-tidy` files are picked up automatically.

### Host tools (clangd, Serena, IDEs)

The container's `build/dev/compile_commands.json` uses container paths (`/src/...`), which host tools cannot open. Configure once, then convert it to a host-path database:

```sh
scripts/dev cmake --preset dev              # writes build/dev/compile_commands.json
python3 tools/host-compile-commands.py      # writes build/dev/host/compile_commands.json
```

The root `.clangd` points clangd at `build/dev/host`. Re-run the script after a reconfigure. It needs only python3 on the host, and it fails with the configure hint when the input is missing.

## Using the library

### With `find_package`

Install it (`cmake --install build/dev --prefix <dir>`), then:

```cmake
find_package(inertia CONFIG REQUIRED COMPONENTS core drogon)   # components: core, fake, drogon
target_link_libraries(app PRIVATE inertia::drogon)             # brings inertia::core with it
```

A package installed without the Drogon adapter (`-DINERTIA_BUILD_DROGON=OFF`) refuses `COMPONENTS drogon` with a clear message. Version compatibility is `SameMinorVersion` while the major version is 0.

### With `add_subdirectory` or `FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(inertia GIT_REPOSITORY <repository url> GIT_TAG <tag>)
FetchContent_MakeAvailable(inertia)
target_link_libraries(app PRIVATE inertia::drogon)
```

As a subproject, tests, examples, install rules and `-Werror` are off. Dependencies (nlohmann_json, and Drogon for the adapter) must be findable by `find_package`; the repository's `vcpkg.json` shows the set.

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

These take a `SessionStore` (use `inertia::drogon::DrogonSession{req->session()}`) and are consumed by the next page render.

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
    std::make_shared<MyFileReader>());           // implement inertia::FileReader (read a file)
config.version = inertia::versionFromVite(vite); // hash of the manifest: a new build triggers a 409 reload
config.vite = vite;                              // first-visit HTML gets the script/link tags
```

While `public/hot` exists (the Vite dev server writes it, see `examples/react-app/frontend/vite.config.ts`) the dev-server tags are emitted; otherwise the tags come from `public/build/.vite/manifest.json`. For SSR build the server bundle (`vite build --ssr src/ssr.tsx`), run it (`bun dist/ssr/ssr.js`, port 13714) and enable it:

```cpp
config.ssr.enabled = true;            // url "http://127.0.0.1:13714", timeout 1500 ms, exceptPaths
// pass inertia::drogon::DrogonHttpClient as the second argument of the Inertia constructor
```

If the SSR server fails or times out, the page falls back to client-side rendering.

## Architecture

Ports and adapters.

- The **core** is pure and synchronous (SSR goes through a callback). It talks to the world through ports: `RequestView`, `Response` (a plain value), `SessionStore`, `HttpClient` (async, for SSR) and `FileReader` (Vite manifest, hot file). A CTest check fails if `drogon` ever appears in `include/inertia/` or `src/`.
- **Adapters** implement the ports for a framework: `adapters/drogon` is the real one, `adapters/fake` is in-memory.
- A **conformance suite** (`tests/conformance`) runs the same HTTP-level protocol scenarios against every adapter.

To write a new adapter (another framework): implement `RequestView` over its request type, `SessionStore` over its session, `HttpClient` over its async client if you want SSR, translate `Response` to and from its response type, and call `Inertia::handleVersion`, `Inertia::render` and `Inertia::finalize` from its hooks. Then add a harness next to `tests/conformance/fake_harness.cpp` and `drogon_harness.cpp` and make the whole suite pass. Props-building code stays portable.

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
| `scripts/dev cmake --workflow --preset dev` | Debug build with `-Werror` and every test (unit, conformance on fake and Drogon, Drogon adapter, `coverage_check`, `core_has_no_drogon`, headers self-contained, package consumer tests). |
| `scripts/dev cmake --workflow --preset core-only` | The core builds and passes without Drogon installed. |
| `scripts/dev cmake --workflow --preset asan` | AddressSanitizer + UBSan (UB fails the test). |
| `scripts/dev-tsan cmake --workflow --preset tsan` | ThreadSanitizer, with instrumented dependencies (`x64-linux-tsan` triplet). |
| `scripts/dev cmake --workflow --preset release` | Release build with strict coverage: no `todo` rule left. |
| `scripts/dev sh -c 'cd e2e && bunx playwright test'` | The official client against the example app (production build). Set `INERTIA_SSR=1` to run again with server-side rendering. Build the example and `bun run build` in `examples/react-app/frontend` first. |
| `python3 -m unittest discover -s tools/tests` | The Python tools (`coverage_check.py`, `check-inertia-version.py`, README table). Also run by CTest as `tools_unit_tests`. |

`pre-commit run --all-files` runs the commit hooks:

- **clang-format** (v23.1.3, the ready-made `pre-commit/mirrors-clang-format` hook) formats the C/C++ files. It runs on the host: no build, no Docker.
- **clang-tidy** runs on `.cpp` files through `scripts/dev` (Docker). It needs a configured `build/dev` but no build: when `build/dev/compile_commands.json` is missing, the hook configures it itself (`cmake --preset dev -DINERTIA_BUILD_EXAMPLES=ON`, examples included) and fails if the configure fails. An existing tree configured without examples is left alone, with a hint.
- A Conventional Commits check on the commit message (`feat(core): ...`, `fix(drogon): ...`).

## Known limitations

- Drogon's `HttpClient` has no cancel API and does not pipeline, so a timed-out SSR request keeps its connection busy until the server answers. `DrogonHttpClient` therefore drops the origin's pooled client after a timeout (at most once per timeout window), and the next render opens a fresh connection instead of queueing behind the hung one. The abandoned request itself still runs to completion in the background, and a persistently hung SSR server costs one new connection per timeout window. Keep `SsrConfig::timeout` short; the page falls back to client-side rendering when it expires.
- Behind a TLS-terminating proxy, `DrogonRequest::fullUrl()` (the `X-Inertia-Location` of the 409 sent on an asset version mismatch) and the `Secure` flag of the CSRF cookie follow the connection, so they say `http` by default. Set `CsrfOptions::trustForwardedHeaders = true` to use the first value of `X-Forwarded-Proto` / `X-Forwarded-Host` instead. Only do that when the proxy always sets those headers: a client that reaches the server directly can forge them. The flag is applied to the requests the adapter builds itself (`install()`, `render()`, `location()`); a `DrogonRequest` you construct takes it as a second constructor argument.
- `toDrogon` turns each `Set-Cookie` into a Drogon cookie, and Drogon keys cookies by name: two cookies with the same name but a different `Path` or `Domain` collapse into one.
- Only the Drogon adapter is provided. The ports make another framework a small piece of work (see Architecture), but none is shipped.
- Laravel-specific features (Precognition, previous-URL storage, exception handler) are out of scope: see the `na` rows above.
- The official Inertia Playwright suite is not reused: its server is a hand-written Express app that builds every page object literally (349 routes), so porting it would test those fixtures rather than this library.
