// The other half of the guard in <inertia/attributes.hpp>: the shared core exports the one function
// that names the C runtime it was built with. Only MSVC and only a shared build have it.
#include <inertia/attributes.hpp>
#include <inertia/core_export.hpp>

#if defined(INERTIA_RUNTIME_GUARD)
extern "C" INERTIA_CORE_EXPORT void INERTIA_RUNTIME_GUARD();

extern "C" void INERTIA_RUNTIME_GUARD() {}
#endif
