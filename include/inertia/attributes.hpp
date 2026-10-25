#pragma once

/// @file
/// Compiler attributes the public headers use, spelled so that every compiler and every C++
/// standard from C++17 accepts them.

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::nonallocating)
/// Marks a function that never allocates memory. Write it after the parameter list, as in
/// `bool same(char a, char b) INERTIA_NONALLOCATING;`. Clang (20 and later) then checks the body
/// (`-Wfunction-effects`) and rejects a call to anything that may allocate. It expands to nothing
/// on other compilers and on Clang versions without function effects.
#define INERTIA_NONALLOCATING [[clang::nonallocating]]
#endif
#endif

#ifndef INERTIA_NONALLOCATING
#define INERTIA_NONALLOCATING
#endif

// Brackets an exported class whose data members are standard library types. MSVC warns (C4251)
// that those types are not part of the DLL interface. The members are private (or plain data) and
// both sides of the boundary must use the same standard library and the same C runtime, so the
// warning does not apply. The runtime condition is enforced, not assumed: see the guard below.
// Expands to nothing elsewhere.
#if defined(_MSC_VER)
/// Starts a class exported from a DLL (see INERTIA_CORE_EXPORT).
#define INERTIA_DLL_INTERFACE_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4251))
/// Ends what INERTIA_DLL_INTERFACE_BEGIN started.
#define INERTIA_DLL_INTERFACE_END __pragma(warning(pop))
#else
#define INERTIA_DLL_INTERFACE_BEGIN
#define INERTIA_DLL_INTERFACE_END
#endif

// The runtime guard. Against the shared core, MSVC code that includes a public header asks the
// linker for the function that only a core DLL built with the same C runtime exports (src/
// runtime_guard.cpp). A consumer built with another runtime (/MT against a /MD DLL, /MDd against
// a release DLL) then fails to link with LNK2001 naming the runtime the DLL expects, instead of
// crashing on a standard library object that crossed the boundary. Static builds have no guard:
// MSVC's own RuntimeLibrary mismatch detection (#pragma detect_mismatch) covers them. The symbols
// are extern "C", so they carry the leading underscore of the x86 decoration and none on x64.
#if defined(_MSC_VER) && !defined(INERTIA_CORE_STATIC_DEFINE)
#if defined(_DLL) && defined(_DEBUG)
#define INERTIA_RUNTIME_GUARD inertia_core_requires_runtime_MDd
#elif defined(_DLL)
#define INERTIA_RUNTIME_GUARD inertia_core_requires_runtime_MD
#elif defined(_DEBUG)
#define INERTIA_RUNTIME_GUARD inertia_core_requires_runtime_MTd
#else
#define INERTIA_RUNTIME_GUARD inertia_core_requires_runtime_MT
#endif
#define INERTIA_DETAIL_STRINGIZE_(name) #name
#define INERTIA_DETAIL_STRINGIZE(name) INERTIA_DETAIL_STRINGIZE_(name)
#if defined(_M_IX86)
#define INERTIA_DETAIL_DECORATION "_"
#else
#define INERTIA_DETAIL_DECORATION ""
#endif
#define INERTIA_DETAIL_INCLUDE_ARG                                                                 \
    "/include:" INERTIA_DETAIL_DECORATION INERTIA_DETAIL_STRINGIZE(INERTIA_RUNTIME_GUARD)
#pragma comment(linker, INERTIA_DETAIL_INCLUDE_ARG)
#endif
