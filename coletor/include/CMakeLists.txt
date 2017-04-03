# Copyright 2014 Dextra

INCLUDE (CheckIncludeFiles)
INCLUDE (CheckFunctionExists)

CHECK_INCLUDE_FILES(inttypes.h    HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES(stdint.h      HAVE_STDINT_H)
CHECK_INCLUDE_FILES(stdlib.h      HAVE_STDLIB_H)
CHECK_INCLUDE_FILES(limits.h      HAVE_LIMITS_H)
CHECK_INCLUDE_FILES(sys/types.h   HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILES(getopt.h      HAVE_GETOPT_H)

CHECK_FUNCTION_EXISTS(strtoimax   HAVE_STRTOIMAX_F)
CHECK_FUNCTION_EXISTS(strptime    HAVE_STRPTIME_F)

# Windows suppport
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  #INCLUDE(win32/CMakeLists.txt)
  #FOREACH (SRCS ${WIN32_SRCS})
  #  LIST(APPEND AWS_SRCS win32/${SRCS})
  #ENDFOREACH (SRCS)
  #INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/win32")
  SET(HAVE_STRUCT_OPTION 1)
ELSE(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  SET(HAVE_STRUCT_OPTION 1)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

# Get git desc (based on http://stackoverflow.com/a/4318642/1596080, and library https://github.com/rpavlik/cmake-modules)
INCLUDE (GetGitRevisionDescription)
git_describe(GIT_DESC --tags --always)

SET(WITH_SSL 0)
MESSAGE(STATUS "configured ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in --> ${CMAKE_CURRENT_BINARY_DIR}/config.h")
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

