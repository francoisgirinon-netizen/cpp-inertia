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
