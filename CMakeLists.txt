cmake_minimum_required(VERSION 3.10...3.31)

# Mapping of automotive task-chains on zonal E/E architectures, in C11.
project(mapping LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

option(MP_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

# results must not depend on fused multiply-add: the golden results are
# compared to the last digit
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffp-contract=off")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(MP_WARNINGS -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes
	-Wmissing-prototypes)

# ---- library -------------------------------------------------------------
add_library(mp STATIC
	src/array.c
	src/assign.c
	src/communication.c
	src/context.c
	src/experiment.c
	src/generator.c
	src/io.c
	src/json.c
	src/let.c
	src/lifecycle.c
	src/model.c
	src/network.c
	src/partition.c
	src/pipeline.c
	src/placement.c
	src/repair.c
	src/rng.c
	src/search.c
	src/sched.c
	src/stats.c
	src/table.c)
target_include_directories(mp PUBLIC include)
target_compile_options(mp PRIVATE ${MP_WARNINGS})
target_link_libraries(mp PUBLIC m)

# design points run in parallel (the results do not depend on it)
find_package(OpenMP)
if(OpenMP_C_FOUND)
	target_link_libraries(mp PUBLIC OpenMP::OpenMP_C)
else()
	message(WARNING "OpenMP not found: experiments run on one thread")
endif()

if(MP_SANITIZE)
	target_compile_options(mp PUBLIC -fsanitize=address,undefined
		-fno-omit-frame-pointer -g)
	target_link_options(mp PUBLIC -fsanitize=address,undefined)
endif()

# a program of tools/ or tests/ linked with the library
function(mp_program name source)
	add_executable(${name} ${source})
	target_compile_options(${name} PRIVATE ${MP_WARNINGS}
		-Wno-missing-prototypes)
	target_link_libraries(${name} mp)
endfunction()

# ---- programs ------------------------------------------------------------
mp_program(mapping tools/mapping.c)	# one application, the algorithms
mp_program(golden tools/golden.c)	# golden results (tests/golden)
mp_program(experiment tools/experiment.c)	# experiments (CSV + summary)
mp_program(export_instance tools/export_instance.c)	# instances for exact/
mp_program(baselines tools/baselines.c)	# heuristic vs SA and GA

# ---- tests ---------------------------------------------------------------
enable_testing()

# unit tests: one executable per file, run by ctest as unit_<name>
foreach(name array rng model sched partition assign placement network
		generator lifecycle io search)
	mp_program(test_${name} tests/test_${name}.c)
	add_test(NAME unit_${name} COMMAND test_${name})
endforeach()

# the whole pipeline must reproduce the committed golden results exactly
add_test(NAME golden
	COMMAND ${CMAKE_SOURCE_DIR}/tests/run_golden.sh $<TARGET_FILE:golden>
		${CMAKE_SOURCE_DIR}/tests/golden)

# the input files give the same results as the built-in generator
add_test(NAME roundtrip
	COMMAND ${CMAKE_SOURCE_DIR}/tests/run_roundtrip.sh
		$<TARGET_FILE:mapping>)

# properties of the model on many random mappings
mp_program(check_invariants tests/check_invariants.c)
add_test(NAME invariants COMMAND check_invariants 20)

# Network Calculus bounds never below the delays of a frame-level
# simulation
mp_program(net_export tests/net_export.c)
add_test(NAME network_bounds
	COMMAND ${CMAKE_SOURCE_DIR}/tests/netsim/run_netsim.sh
		$<TARGET_FILE:net_export> ${CMAKE_SOURCE_DIR}/tests/netsim/netsim.py)

# end-to-end bounds never below the delays of a full-system simulation
add_test(NAME system_bounds
	COMMAND ${CMAKE_SOURCE_DIR}/tests/syssim/run_syssim.sh
		$<TARGET_FILE:mapping> ${CMAKE_SOURCE_DIR}/tests/syssim/syssim.py)

# the results of an experiment do not depend on the number of threads
add_test(NAME experiment_threads
	COMMAND ${CMAKE_SOURCE_DIR}/tests/run_threads.sh
		$<TARGET_FILE:experiment>)

# the exact formulation agrees with the tool (needs OR-Tools)
execute_process(COMMAND python3 -c "import ortools"
		RESULT_VARIABLE MP_NO_ORTOOLS OUTPUT_QUIET ERROR_QUIET)
if(MP_NO_ORTOOLS EQUAL 0)
	add_test(NAME exact
		COMMAND python3 ${CMAKE_SOURCE_DIR}/exact/gap.py
			$<TARGET_FILE:export_instance> --sizes 20,40 --seeds 2
			--time-limit 30 --configs default,constraints
			--out ${CMAKE_BINARY_DIR}/exact_test.csv)
endif()
