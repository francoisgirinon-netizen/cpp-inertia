# Writing an adapter

An adapter connects the framework-free core (`inertia::core`) to one HTTP framework. The library
ships adapters for Drogon (`inertia::drogon`) and cpp-httplib (`inertia::httplib`); any other
framework needs one of its own. Everything an adapter needs is public API: the shipped adapters
include nothing from `inertia/detail`, and neither should yours.

The simplest reference is `adapters/httplib` (synchronous handlers, an in-memory session store).
`adapters/drogon` shows an asynchronous framework with a built-in session. The test-only adapter in
`adapters/fake` shows the bare minimum.

An adapter is done when the **conformance kit** passes against it: the same protocol scenarios the
shipped adapters run (see [Running the conformance kit](#running-the-conformance-kit)).

## The five ports

All ports are abstract classes in `inertia/ports/`, with protected special members: implement them,
do not slice them.

### `RequestView` (required)

A read-only view of the framework's request.

| Member | Contract |
| --- | --- |
| `method()` | The HTTP method in **uppercase** (`"GET"`, `"POST"`, ...). The core compares it verbatim. |
| `url()` | Path and query **exactly as received** (`/users?page=2`). It becomes `page.url`, so do not decode or normalize it. |
| `fullUrl()` | `scheme://host` followed by `url()`. Used for absolute URLs (SSR, `Referer`-less redirects). |
| `header(name)` | The value of a header, looked up **case-insensitively**; `nullopt` when absent. Join repeated lines with `", "`. |

The view usually borrows the framework's request: keep it alive while the view is used. It does not
need to be thread safe (a request is handled by one thread at a time).
`inertia::headers::iequals` (`inertia/headers.hpp`) does the case-insensitive comparison.

### `Response` (a value, not a port)

`inertia::Response` is a plain struct (`status`, an ordered list of `headers`, `body`). The
adapter's job is the translation to and from the framework's response type. Keep **every** header
line: several `Set-Cookie` headers must all survive, and header order is preserved. Drop
`Content-Length` if your framework frames the body itself (the httplib adapter does).

### `SessionStore` (required)

Key/value storage for the current visitor, with `Json` values.

| Member | Contract |
| --- | --- |
| `get(key)` | The value, or `nullopt`. Must not modify the session. |
| `put(key, value)` | Stores or replaces the value. |
| `pull(key)` | `get` then erase, atomically: the value is consumed once. |

What the core expects of the session:

- **It persists between requests** of the same visitor (a cookie-backed session, as every web
  framework has one). Redirect-after-POST flows depend on it: the POST stores flash data and errors,
  the following GET renders and consumes them.
- The core reads and writes these keys, and you never need to touch them yourself:
  `inertia.errors` (validation errors per bag), `inertia.flash_data`, `inertia.clear_history` and
  `inertia.preserve_fragment`. The helpers `withErrors`, `flash`, `clearHistory` and
  `preserveFragment` (`inertia/session.hpp`) write them; the next page render consumes them
  (`pull`), so each shows up exactly once. Keep other session entries untouched.
- Values are JSON; if your session only stores strings, store `dump()` and parse back.
- A session may be used by concurrent requests of the same visitor (a browser fires several at
  once). Make individual operations atomic (the httplib adapter locks per session).
- A request that never touches the session should not need to create one (the httplib adapter
  creates sessions lazily, so a 404 or a health check sets no cookie).

### `HttpClient` (optional, for SSR)

Needed only when `Config::ssr.enabled` is set; pass it as the second argument of the `Inertia`
constructor. Both methods are asynchronous in shape:

```cpp
void post(std::string url, std::string jsonBody, std::chrono::milliseconds timeout,
          std::function<void(HttpResult)> done);
void get(std::string url, std::chrono::milliseconds timeout,
         std::function<void(HttpResult)> done);
```

Call `done` **exactly once**, never throw. A failure is reported as an `HttpResult` with `status`
0 (or the real status) and `error` set. `timeout` bounds the whole exchange. `done` may run on any
thread; a blocking client may simply call it before returning (the httplib adapter does, the Drogon
adapter uses the framework's event loop). Without an `HttpClient` the core never server-renders and
serves the client-side page.

### `FileReader` (optional, for Vite)

`std::optional<std::string> read(const std::filesystem::path &)`: the whole content of a file, or
`nullopt` when it cannot be read. `Vite` uses it for the manifest and the hot file; `Inertia` never
touches the file system itself. It must be callable from several threads (it is `const`).

## The request pipeline

For every request, the adapter runs three steps, in this order, around the application's handler:

1. **`Inertia::handleVersion(req)` before routing.** It answers a GET Inertia request whose
   `X-Inertia-Version` is stale with `409` plus `X-Inertia-Location`. If it returns a response, send
   that response and skip the handler; otherwise continue.
2. **The handler** of the matched route. It calls `Inertia::render(...)`, `Inertia::location(...)`,
   `inertia::redirect(...)`, `inertia::back(req)` and the session helpers, and ends with exactly one
   `Response`.
3. **`Inertia::finalize(req, res)` on every response**, including those of step 1, of 404s and of
   errors. It adds `Vary: X-Inertia` (merged with an existing `Vary`).

Frameworks with hooks map them onto these steps (pre-routing and post-routing handlers in httplib,
pre-handling and post-handling advices in Drogon). A framework without hooks gets a small
dispatcher or middleware that does the three steps, like the in-process one in
`tests/package/main_custom.cpp`.

A 419 from the CSRF check (below) is the adapter's own answer; the core never produces one.

## Rendering

```cpp
void render(const RequestView &req, SessionStore &session, std::string component, Props props,
            std::function<void(Response)> done, RenderOptions opts = {}) const;
```

- `component` and `props` are taken **by value** and consumed: the page is built, and every prop
  resolved, before `render()` returns. You may destroy the request and the props the moment it
  returns.
- `done` receives the finished `Response` (JSON for an Inertia request, the HTML page otherwise).
  **It may run later and on another thread**: with SSR enabled, it runs when the SSR server answered
  or timed out. It must own everything it uses: capture the framework's response callback by value,
  never a reference to a stack variable (unless you block until `done` ran, as the httplib adapter
  does).
- An exception thrown by a prop resolver escapes from `render()`: turn it into a 500 in the adapter.
- `done` is called exactly once. A synchronous framework can wait for it; an asynchronous one hands
  the response to its own callback.

`Inertia::page()` returns just the page object as JSON, without a response; it is handy in tests.

## CSRF

The core does not check CSRF tokens: the framework's session decides how, so it is the adapter's
job. The two shipped adapters implement the convention that Axios (which the Inertia client uses)
follows, and it is a good default for yours:

- On the first response of a session, store a random token in the session and send it in the
  readable (not `HttpOnly`) cookie `XSRF-TOKEN`.
- For every method except `GET`, `HEAD` and `OPTIONS`, require the header `X-XSRF-TOKEN` to equal
  the session's token, compared in constant time; otherwise answer `419`.
- Allow a list of exempt paths (webhooks, the SSR server cannot hold a token), and a way to switch
  the check off (the conformance suites send no token).
- Send the cookie on later responses when the client does not have the current token, and give
  `Secure` cookies only for requests that arrived over HTTPS.

If your framework already has CSRF protection, use it and skip all of this.

## Threading

- `Inertia` is **immutable after setup**: register `share`, `shareWith` and `shareOnce` at startup,
  before the first request, and share the instance as `std::shared_ptr<const Inertia>`. Concurrent
  calls then need no locking.
- Per-request state (the `RequestView`, the `SessionStore` handle, the `Response`) lives on the
  stack of the request, or in a heap object owned by the asynchronous callback, never in globals.
- Mind the `render()` callback thread (above), and keep the `HttpClient` thread safe.

## Running the conformance kit

The kit is a Catch2 suite of HTTP-level scenarios (one test case per observable protocol rule).
It is the component `conformance` of the installed package (`inertia::conformance`), and it is also
available through `add_subdirectory`/`FetchContent`. It needs Catch2 v3.

You provide one function, `send()`, declared in `<inertia/conformance/harness.hpp>`:

```cpp
namespace inertia::conformance {
TestResponse send(const TestRequest &req); // one HTTP exchange against your application
}
```

1. **Build the test application.** `<inertia/conformance/routes.hpp>` gives you
   `makeConformanceApp()` (an `Inertia` configured for the suite: version `"v1"`, SSR off) and
   `conformanceRoutes()`, a list of `Route{method, path, handler}`. Register each route in your
   framework so that a request to `path` with `method` calls
   `route.handler(request, session, app, done)`, where `request` is your `RequestView`, `session`
   your `SessionStore`, `app` the `Inertia`, and `done` produces the framework's response. Run the
   pipeline above around it, with **CSRF disabled**. Every handler calls `done` synchronously.
2. **Implement `send()`.** Turn the `TestRequest` (`method`, `url` with the query, `headers`,
   `body`) into an exchange with that application and return the `TestResponse` (`status`,
   `headers`, `body`). The contract:
   - **The session persists across calls** to `send()`, as if one browser did them all: keep a
     cookie jar if you go over real HTTP, or one session object if you stay in-process.
   - **`POST /__reset` starts a fresh session** (forget the cookie jar or replace the session
     object) and returns `204`. It never reaches the application. The scenarios call it before each
     test.
   - Return the response as the application produced it: do not follow redirects, and keep every
     header.
3. **Add the test executable** with the helper that the package installs:

   ```cmake
   find_package(inertia CONFIG REQUIRED COMPONENTS core conformance)

   inertia_add_conformance_test(my_adapter_conformance
       my_harness.cpp                # the file(s) that define send()
       LINK my::adapter my::framework)  # whatever the harness needs
   ```

   It creates the executable (the kit's scenarios, your sources and the libraries after `LINK`),
   links `Catch2::Catch2WithMain` and registers every scenario with CTest (`catch_discover_tests`,
   names `my_adapter_conformance: <scenario>`, label `conformance`). Run it with
   `ctest -L conformance`, or run the executable directly.

   The scenarios are compiled in **your** executable (they ship as installed source files, wired
   through the `inertia::conformance` target), so that the linker cannot drop them and they are
   built with your flags and sanitizers. If `--list-tests` shows nothing, the target did not get
   them: check that you went through `inertia_add_conformance_test`.
4. **Make it green**, then keep it in CI. A failing scenario names the protocol rule it checks
   (`[protocol:<id>]` tags, listed in `docs/protocol-coverage.toml`).

A complete, small example that uses only the installed package is `tests/package/main_custom.cpp`:
an in-process adapter with its own `RequestView`, `SessionStore` and dispatcher. The
`package.custom.*` tests install the library, configure that consumer against the installed
package, build it and run the whole kit. The real thing over HTTP is `tests/conformance/httplib_harness.cpp`.
