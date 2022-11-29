add_library(bpp-if
        SHARED
        mapping.cpp generator.cpp print.cpp bfdu_f.cpp wfdu_f.cpp bench.cpp)

install (TARGETS bpp-if
        DESTINATION lib)

file(
        GLOB
        headers
        *.h
        )

install(FILES ${headers}
        DESTINATION include/${CMAKE_PROJECT_NAME})
