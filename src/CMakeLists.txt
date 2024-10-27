add_executable(clox
        main.c
        common.h
        chunk.h
        chunk.c
        memory.h
        memory.c
        debug.h
        debug.c
        value.h
        value.c
)

target_link_libraries(clox
        PRIVATE
        clox_project_options
)
