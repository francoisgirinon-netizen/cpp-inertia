# Loopback socket helpers shared by the adapter tests (BSD sockets, Winsock on Windows).
add_library(inertia_test_loopback STATIC loopback.cpp)
target_include_directories(inertia_test_loopback PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(inertia_test_loopback PRIVATE inertia_warnings)
if(WIN32)
    target_link_libraries(inertia_test_loopback PRIVATE ws2_32)
endif()
inertia_enable_iwyu(inertia_test_loopback)
