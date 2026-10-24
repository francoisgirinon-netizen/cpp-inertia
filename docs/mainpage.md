# cpp-inertia {#mainpage}

cpp-inertia is a C++ implementation of the server side of the Inertia protocol. It is written against
ports, so it does not depend on a particular web framework.

Start with the `inertia::Inertia` class: it builds Inertia responses (page objects, partial reloads,
shared props, redirects, asset versioning) from a request and a set of props.

The framework-facing code lives behind ports, small interfaces the core calls to read a request and
write a response. Adapters implement those ports for a given framework: Drogon, cpp-httplib and a
fake adapter for tests.

To support another framework, read `docs/writing-an-adapter.md` and run the conformance kit in
`inertia/conformance` against your adapter.
