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
// both sides of the boundary must use the same standard library and the same C runtime anyway,
// which the install documents, so the warning does not apply. Expands to nothing elsewhere.
#if defined(_MSC_VER)
/// Starts a class exported from a DLL (see INERTIA_CORE_EXPORT).
#define INERTIA_DLL_INTERFACE_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4251))
/// Ends what INERTIA_DLL_INTERFACE_BEGIN started.
#define INERTIA_DLL_INTERFACE_END __pragma(warning(pop))
#else
#define INERTIA_DLL_INTERFACE_BEGIN
#define INERTIA_DLL_INTERFACE_END
#endif
