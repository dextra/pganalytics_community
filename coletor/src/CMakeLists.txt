# Copyright 2014 Dextra, all rights reserved

# set the include path for all files in the src directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# add files to compile
set(APP_SRCS
	UserConfig.cpp
	ConfigParser.cpp
	ServerInfo.cpp
	StorageManager.cpp
	collectors.cpp
	StateManager.cpp
	LogManager.cpp
	push.cpp
	)

# Windows suppport
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	enable_language(RC)
	set(APP_SRCS ${APP_SRCS}
		win/getopt_long.c
		win/win32error.cpp
		win/stacktrace.cpp
		)
	set(pganalytics_SRCS ${pganalytics_SRCS}
		win/pganalytics.rc
		)
	set(APP_SRCS ${APP_SRCS}
		collectors_sysstat_win.cpp
		)
	set(requiredlibs ${requiredlibs} imagehlp) # Used by stacktrace.cpp
else(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(APP_SRCS ${APP_SRCS}
		collectors_sysstat.cpp
		)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

# Util
add_subdirectory(util)

###
# Static linking hacks...
###

if(NOT WIN32) # No need to build statically on Windows
	# XXX: Besides "libcurl", "libaws" and "libpq", we need all other libraries to be in standard SO locations
	# Add our version of libcurl:
	link_directories("${CMAKE_BINARY_DIR}/libs/curl/lib/.libs/")
	include_directories("${CMAKE_BINARY_DIR}/libs/curl/include/")
	include_directories("${CMAKE_SOURCE_DIR}/libs/curl-7.41.0/include/")
	# And our version of libxml2 (required by libaws):
	link_directories("${CMAKE_BINARY_DIR}/libs/libxml2/.libs/")
	include_directories("${CMAKE_BINARY_DIR}/libs/libxml2/include/")
	include_directories("${CMAKE_SOURCE_DIR}/libs/libxml2-2.9.2/include/")
	# The same for OpenSSL
	link_directories("${CMAKE_BINARY_DIR}/libs/openssl/")
	include_directories("${CMAKE_BINARY_DIR}/libs/openssl/include/")
	# Now, libaws
	include_directories("${CMAKE_BINARY_DIR}/libs/libaws/include/")
	include_directories("${CMAKE_SOURCE_DIR}/libs/libaws/include/")
	link_directories("${CMAKE_BINARY_DIR}/libs/libaws/src/")
	# Redefine requiredlibs, for easy use of static build (as the order matters)
	set(requiredlibs aws xml2 pq pthread curl ssl crypto z rt dl)
	#set(requiredlibs aws "${CMAKE_BINARY_DIR}/libs/libxml2/.libs/libxml2.a" pq pthread "${CMAKE_BINARY_DIR}/libs/curl/lib/.libs/libcurl.a" "${CMAKE_BINARY_DIR}/libs/openssl/libssl.a" "${CMAKE_BINARY_DIR}/libs/openssl/libcrypto.a" z rt dl)
	# Linker flags
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static")
endif(NOT WIN32) # No need to build statically on Windows

###
# pgAnalytics implementation library.
#
# Include everything except main file, this is required for test binary,
# so it can have everything but implement its own main.
# OBS: You must link pgautil too
###
add_library(pganalytics_impl STATIC ${pganalytics_SRCS} ${APP_SRCS})
#target_link_libraries(pganalytics_impl pgautil)

###
# pgAnalytics agent executable (statically linked)
###

add_executable(pganalytics pganalytics.cpp)
target_link_libraries(pganalytics pganalytics_impl pgautil ${requiredlibs})

###
# Make it static
###
#set_target_properties(pganalytics PROPERTIES LINK_SEARCH_START_STATIC 1)
#set_target_properties(pganalytics PROPERTIES LINK_SEARCH_END_STATIC 1)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/pganalytics DESTINATION bin)

# Backend
if(NOT WIN32)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/backend/CMakeLists.txt")
	add_subdirectory(backend)
endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/backend/CMakeLists.txt")
endif(NOT WIN32)

# Windows Agent
if(WIN32)
	add_subdirectory(win/agent)
endif(WIN32)

