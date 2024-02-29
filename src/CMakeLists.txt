add_library(mapping
        SHARED
        mapping.cpp generator.cpp print.cpp bfdu.cpp wfdu.cpp ffdu.cpp stats.cpp
        file.cpp sched_analysis.cpp placement.cpp model.cpp let.cpp ucases.cpp)

install (TARGETS mapping
        DESTINATION lib)

file(
        GLOB
        headers
        *.h
        )

install(FILES ${headers}
        DESTINATION include/${CMAKE_PROJECT_NAME})
