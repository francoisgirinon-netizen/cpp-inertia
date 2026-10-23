# x64-linux with ThreadSanitizer, used by the tsan preset. TSan only understands the
# synchronization it can see: with Drogon/trantor built uninstrumented it reports races in
# their lock-free queues and function-local statics that are not real. Building the
# dependencies with the same flags removes those false positives at the source.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_C_FLAGS "-fsanitize=thread -fno-omit-frame-pointer")
set(VCPKG_CXX_FLAGS "-fsanitize=thread -fno-omit-frame-pointer")
set(VCPKG_LINKER_FLAGS "-fsanitize=thread")
