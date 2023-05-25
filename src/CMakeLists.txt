add_library(bpp-ifc
        SHARED
        mapping.cpp generator.cpp print.cpp bfdu_f.cpp wfdu_f.cpp frst_f.cpp
        exp.cpp sched_analysis.cpp optimization.cpp model.cpp let.cpp
        communication.cpp)

install (TARGETS bpp-ifc
        DESTINATION lib)

file(
        GLOB
        headers
        *.h
        )

install(FILES ${headers}
        DESTINATION include/${CMAKE_PROJECT_NAME})
