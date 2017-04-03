# Copyright 2014 Dextra, all rights reserved

# Get selected regexp library
include(${CMAKE_SOURCE_DIR}/cmake_addon/SelectRegexpLibrary.cmake)

set(BACKEND_SRCS
	pganalytics-imp.cpp
	LogParser.cpp
	${REGEXP_EXTRA_SRC}
	)


add_library(pganalytics_importer_impl STATIC ${BACKEND_SRCS})
#target_link_libraries(pganalytics_importer_impl pganalytics_impl)
add_executable(pganalytics-importer pganalytics-imp-main.cpp)
target_link_libraries(pganalytics-importer pganalytics_importer_impl pganalytics_impl pgautil ${requiredlibs} ${REGEXP_LIBRARY})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/pganalytics-importer DESTINATION bin)

