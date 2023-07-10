add_library(mapping
        SHARED
        mapping.cpp generator.cpp print.cpp bfdu_f.cpp wfdu_f.cpp ffdu_f.cpp
        exp.cpp sched_analysis.cpp optimization.cpp model.cpp let.cpp
        communication.cpp)

install (TARGETS mapping
        DESTINATION lib)

file(
        GLOB
        headers
        *.h
        )

install(FILES ${headers}
        DESTINATION include/${CMAKE_PROJECT_NAME})
