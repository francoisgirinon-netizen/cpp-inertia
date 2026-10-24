# cmake -DCOMPILE_COMMANDS=<compile_commands.json> -P check_flags.cmake
file(READ "${COMPILE_COMMANDS}" _commands)
if(_commands MATCHES "-Werror")
    message(FATAL_ERROR "-Werror leaked into a subproject build")
endif()
if(NOT _commands MATCHES "-Wall")
    message(FATAL_ERROR "the library lost its own warning flags")
endif()
