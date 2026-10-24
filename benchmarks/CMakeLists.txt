# Benchmarks: not part of the tests and not run by ctest. Configure with
# -DINERTIA_BUILD_BENCHMARKS=ON on a Release build and run the executable directly.
find_package(Catch2 CONFIG REQUIRED)

add_library(inertia_bench_fixtures STATIC fixtures.cpp)
target_link_libraries(inertia_bench_fixtures PUBLIC inertia::core inertia::fake
                      PRIVATE inertia_warnings)
target_compile_features(inertia_bench_fixtures PRIVATE cxx_std_20)

add_executable(inertia_benchmarks request_bench.cpp)
target_link_libraries(inertia_benchmarks PRIVATE inertia_bench_fixtures Catch2::Catch2WithMain
                                                 inertia_warnings)
target_compile_features(inertia_benchmarks PRIVATE cxx_std_20)
