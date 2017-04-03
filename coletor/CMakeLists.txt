# Copyright 2014 Dextra, all rights reserved

project(pganalytics)

cmake_minimum_required(VERSION 2.6)

# prohibit in-source-builds
if (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    message(STATUS "In-source-builds are not allowed")
    message(STATUS "Clean your source directory (e.g. delete the CMakeCache.txt file) and use: mkdir build/; cd build/; cmake ../")
    message(FATAL_ERROR "Please create a separate build directory and call CMake again")
endif (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_addon")

# set the default build type to debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

option(BUILD_TESTS "Build all tests." ON)

# Look for required libraries
set(requiredlibs)

# Find PostgreSQL
find_library(PQ_LIB pq)
find_path(PQ_PATH libpq-fe.h PATH_SUFFIXES postgresql pgsql)
if(PQ_LIB AND PQ_PATH)
	include_directories(${PQ_PATH})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${PQ_LIB}")
	if(WIN32)
		set(requiredlibs ${requiredlibs} "libpq")
	else(WIN32)
		set(requiredlibs ${requiredlibs} "pq")
	endif(WIN32)
else(PQ_LIB AND PQ_PATH)
	message(FATAL_ERROR "Could not find the PostgreSQL client library (libpq) and development files.")
endif(PQ_LIB AND PQ_PATH)

### # Find pthread (required by libpq, and must be included after it for static linking)
### find_library(PTHREAD_LIB pthread)
### find_path(PTHREAD_PATH pthread.h)
### if(PTHREAD_LIB AND PTHREAD_PATH)
### 	include_directories(${PTHREAD_PATH})
### 	get_filename_component(PTHREAD_LIB_DIR ${PTHREAD_LIB} PATH)
### 	link_directories(${PTHREAD_LIB_DIR})
### 	if(WIN32)
### 		set(requiredlibs ${requiredlibs} "libpthread")
### 	else(WIN32)
### 		set(requiredlibs ${requiredlibs} "pthread")
### 	endif(WIN32)
### 	message(STATUS "Found pthread at ${PTHREAD_LIB_DIR} and ${PTHREAD_PATH}")
### else(PTHREAD_LIB AND PTHREAD_PATH)
### 	message(FATAL_ERROR "Could not find pthreads library and development files.")
### endif(PTHREAD_LIB AND PTHREAD_PATH)

# Find libaws
find_library(LIBAWS_LIB libaws)
find_path(LIBAWS_PATH libaws/aws.h PATH_SUFFIXES libaws)
if(LIBAWS_LIB AND LIBAWS_PATH)
	include_directories(${LIBAWS_PATH})
	include_directories("${CMAKE_CURRENT_BINARY_DIR}/libs/libaws/include/")
	link_directories("${CMAKE_BINARY_DIR}/libs/libaws/src/")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${LIBAWS_LIB}")
	if(WIN32)
		include_directories("${CMAKE_CURRENT_SOURCE_DIR}/libs/libaws/src/win32/")
		include_directories("${CMAKE_CURRENT_SOURCE_DIR}/libs/win32/pthread/include/")
		set(requiredlibs ${requiredlibs} "libaws")
	else(WIN32)
		set(requiredlibs ${requiredlibs} "aws")
	endif(WIN32)
	install(PROGRAMS "${CMAKE_BINARY_DIR}/libs/libaws/src/libaws.so.0.9.3" DESTINATION bin)
	message(STATUS "Using libaws: ${LIBAWS_LIB} and ${LIBAWS_PATH}")
else(LIBAWS_LIB AND LIBAWS_PATH)
	message(FATAL_ERROR "Could not find libaws library and development files.")
endif(LIBAWS_LIB AND LIBAWS_PATH)

# Find ZLib
message(STATUS "zlib: ${ZLIB_LIB} and ${ZLIB_INCLUDE_DIR}")
find_library(ZLIB_LIB z zlib1 libz zlib zdll)
find_path(ZLIB_INCLUDE_DIR zlib.h)
if(ZLIB_LIB AND ZLIB_INCLUDE_DIR)
	include_directories(${ZLIB_INCLUDE_DIR})
	get_filename_component(ZLIB_LIB_DIR ${ZLIB_LIB} PATH)
	link_directories(${ZLIB_LIB_DIR})
	if(WIN32)
		set(requiredlibs ${requiredlibs} "zlib1")
	else(WIN32)
		set(requiredlibs ${requiredlibs} "z")
	endif(WIN32)
	message(STATUS "Found zlib at ${ZLIB_LIB_DIR} and ${ZLIB_INCLUDE_DIR}")
else(ZLIB_LIB AND ZLIB_INCLUDE_DIR)
	message(STATUS "zlib: ${ZLIB_LIB} and ${ZLIB_INCLUDE_DIR}")
	# Try with FIND_PACKAGE
	find_package(ZLIB)
	if(ZLIB_FOUND)
		include_directories(${ZLIB_INCLUDE_DIR})
		set(requiredlibs ${requiredlibs} ${ZLIB_LIBRARIES} )
	else(ZLIB_FOUND)
		message(FATAL_ERROR "Could not find the ZLIB library and development files.")
	endif(ZLIB_FOUND)
endif(ZLIB_LIB AND ZLIB_INCLUDE_DIR)

include(CMakeConfiguration.txt)
include(CMakeCompiler.txt)

set(APP_MAJOR_VERSION "0")
set(APP_MINOR_VERSION "0")
set(APP_PATCH_VERSION "0")

# set the include path to the API headers
# note that there also some generated headers files in the build directory
include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(AFTER ${CMAKE_CURRENT_BINARY_DIR}/include)

# If we want to build static binaries:
### if(WIN32)
### 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-rpath=$ORIGIN")
### else(WIN32)
### 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-rpath=$ORIGIN,--enable-new-dtags")
### endif(WIN32)

#set(BUILD_SHARED_LIBS OFF)
###set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static")
#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} " -static-libstdc++")
##set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

#set(CMAKE_STATIC_LINKER_FLAGS "-static-libgcc -static")

# Tell CMake to process the sub-directories
add_subdirectory(include)
add_subdirectory(src)

message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")

# Tests
if(BUILD_TESTS)
	add_subdirectory(libs/gtest-1.7.0)
	include_directories(SYSTEM ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
	add_subdirectory(tests)
	enable_testing()
	set_directory_properties(PROPERTIES TEST_INCLUDE_FILE "${CMAKE_BINARY_DIR}/tests/tests.cmake")
endif(BUILD_TESTS)

# Install sample conf file
install(FILES ${CMAKE_SOURCE_DIR}/pganalytics.sample.conf DESTINATION etc)

# Add CPack support
configure_file(${CMAKE_SOURCE_DIR}/packages/deb/preinst.in ${CMAKE_BINARY_DIR}/packages/deb/preinst)
set(CPACK_GENERATOR "STGZ;TGZ;DEB")
include(CPackConfig.cmake)
include(CPack)

# we may use libaws example for the TODOs above

