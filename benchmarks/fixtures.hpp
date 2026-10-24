#pragma once

// Realistic request fixtures shared by the benchmarks: a "typical" page of about 17 KB (20 own
// props and 6 shared ones, nested objects, a 50-item user list, three 1 KB strings that need
// script escaping) and a Vite manifest of 30 entries.

#include <inertia/inertia.hpp>
#include <inertia/json.hpp>
#include <inertia/props.hpp>

#include <string>

namespace inertia::bench {

/// A user object with nested data.
[[nodiscard]] Json user(int index);

/// The props of the typical page. `largeItems` adds a list of that many posts (the large page).
[[nodiscard]] Props makeProps(int largeItems = 0);

/// Shares the typical set of shared props (and one per-request prop) with `inertia`.
void shareTypical(Inertia &inertia);

/// A Vite manifest with `entries` chunks and a realistic import graph.
[[nodiscard]] std::string manifestText(int entries = 30);

} // namespace inertia::bench
