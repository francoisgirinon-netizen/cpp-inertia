# inertia_enable_iwyu(<target>...)
#
# With INERTIA_IWYU=ON, makes the compilation of each target run include-what-you-use and fail on
# any suggestion (-Xiwyu --error). Without it, does nothing. The command is set per target, never
# through CMAKE_CXX_INCLUDE_WHAT_YOU_USE: that variable would also reach the targets of a project
# embedding this one. Third-party quirks (nlohmann, Catch2, Drogon, ...) are mapped in .iwyu.imp.
include_guard(GLOBAL)

if(INERTIA_IWYU)
    find_program(INERTIA_IWYU_PROGRAM include-what-you-use)
    if(NOT INERTIA_IWYU_PROGRAM)
        message(FATAL_ERROR
            "INERTIA_IWYU is ON but include-what-you-use was not found; install it "
            "(it is pinned in mise.toml and installed in the dev image) or turn the option OFF")
    endif()
    set(_inertia_iwyu_command
        ${INERTIA_IWYU_PROGRAM}
        -Xiwyu --error
        -Xiwyu --no_fwd_decls
        -Xiwyu --mapping_file=${CMAKE_CURRENT_LIST_DIR}/../.iwyu.imp)
endif()

function(inertia_enable_iwyu)
    if(INERTIA_IWYU)
        set_target_properties(${ARGN} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${_inertia_iwyu_command}")
    endif()
endfunction()
