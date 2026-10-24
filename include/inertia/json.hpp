#pragma once

#include <nlohmann/json.hpp>

namespace inertia {

/// JSON value type used for page objects and props. It keeps object keys in insertion order, so
/// the emitted page has a stable key order.
using Json = nlohmann::ordered_json;

} // namespace inertia
