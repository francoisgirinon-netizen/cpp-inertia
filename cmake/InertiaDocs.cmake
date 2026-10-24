# docs: the Doxygen API reference, built from docs/Doxyfile into <build>/docs/html.
# Only included with -DINERTIA_BUILD_DOCS=ON. The target is not part of `all`:
#   cmake --build <dir> --target docs
find_package(Doxygen REQUIRED)

add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} -E env
            "INERTIA_PROJECT_NAME=${PROJECT_NAME}"
            "INERTIA_PROJECT_VERSION=${PROJECT_VERSION}"
            "INERTIA_DOCS_OUTPUT_DIR=${CMAKE_BINARY_DIR}/docs"
            ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Building the API reference with Doxygen"
    VERBATIM)
