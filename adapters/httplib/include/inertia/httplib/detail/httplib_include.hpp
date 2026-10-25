#pragma once

/// @file
/// The one place that includes cpp-httplib. Include this header, never <httplib.h> directly.

// cpp-httplib has code after a `throw` (in its Client constructor), which MSVC reports as C4702
// (unreachable code) at /W4. C4702 is raised by the compiler back end, and Microsoft documents that
// such warnings (usually C47xx) "aren't affected by the /external options": marking the vcpkg
// include directory as external does not silence it, so it is disabled around this one include.
// https://learn.microsoft.com/en-us/cpp/build/reference/external-external-headers-diagnostics
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include <httplib.h> // IWYU pragma: export
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
