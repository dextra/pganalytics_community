# Copyright 2014 Dextra, all rights reserved

# add files to compile
set(UTIL_SRCS
	MainApplication.cpp
	fs.cpp
	fs_stat.cpp
	fs_dirent.cpp
	streams.cpp
	string.cpp
	time.cpp
	log.cpp
	)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(UTIL_SRCS ${UTIL_SRCS}
		../win/win32error.cpp
		)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

add_library(pgautil ${UTIL_SRCS})

