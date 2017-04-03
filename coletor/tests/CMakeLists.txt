# Copyright 2014 Dextra, all rights reserved

set(GTEST_BINARIES)

# set the include path for all files in the src directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Tests
enable_testing()

# Get selected regexp library
include(${CMAKE_SOURCE_DIR}/cmake_addon/SelectRegexpLibrary.cmake)

# Test executable
add_executable(pganalytics-test test.cpp)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(pganalytics-test pganalytics_impl pgautil gtest ${requiredlibs} ${REGEXP_LIBRARY})
else(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(pganalytics-test pganalytics_impl pgautil pganalytics_importer_impl gtest ${requiredlibs} ${REGEXP_LIBRARY})
endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
set(GTEST_BINARIES ${GTEST_BINARIES} pganalytics-test)

set(test_cmds)
foreach(test_cmd ${GTEST_BINARIES})
	set(test_cmds "${test_cmds} ${test_cmd}")
endforeach(test_cmd ${GTEST_BINARIES})

add_custom_command(#TARGET mytest POST_BUILD
	OUTPUT tests.cmake
	DEPENDS process_test_suite.cmake ${GTEST_BINARIES}
	COMMAND ${CMAKE_COMMAND}
		-Dbinary_dir=${CMAKE_CURRENT_BINARY_DIR}
		-Dtest_cmds=${test_cmds}
		-Doutput_file=${CMAKE_CURRENT_BINARY_DIR}/tests.cmake
		-P ${CMAKE_CURRENT_SOURCE_DIR}/process_test_suite.cmake
	VERBATIM
	)
# XXX: tests.cmake is read again at coletor/CMakeLists.txt, with: set_directory_properties(PROPERTIES TEST_INCLUDE_FILE "${CMAKE_BINARY_DIR}/tests/tests.cmake")
add_custom_target(generate_test_suite ALL DEPENDS tests.cmake)

add_custom_target(check
	COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
	DEPENDS ${GTEST_BINARIES}
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)

