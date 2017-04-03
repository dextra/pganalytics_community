# Copyright 2008 28msec, Inc.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
PROJECT(libaws)
SET(LIBCRYPTO "crypto")
SET(LIBPTHREADS "pthread")
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  INCLUDE(CMakeWindows.cmake)
  SET(LIBCRYPTO "")
  SET(LIBPTHREADS "${PTHREAD_LIB_PATHS}/pthreadVCE.lib")
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

# ctest support
ENABLE_TESTING()
INCLUDE(CTest)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
IF(COMMAND cmake_policy)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)


# prohibit in-source-builds
IF (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    MESSAGE(STATUS "In-source-builds are not allowed")
    MESSAGE(STATUS "Clean your source directory (e.g. delete the CMakeCache.txt file)")
    MESSAGE(FATAL_ERROR "Please create a separate build directory and call CMake again")
ENDIF (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

# set the default build type to debug
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

# Look for required libraries
SET(requiredlibs)

FIND_LIBRARY(CURL_LIB libcurl)
FIND_PATH(CURL_INCLUDE_DIR curl.h PATH_SUFFIXES curl)
IF(CURL_LIB AND CURL_INCLUDE_DIR)
	INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
	LINK_DIRECTORIES(${CURL_LIB})
	SET(requiredlibs ${requiredlibs} "libcurl")
	MESSAGE(STATUS "Found libcUrl: ${CURL_LIB} and ${CURL_INCLUDE_DIR}")
ELSE(CURL_LIB AND CURL_INCLUDE_DIR)
	# Try with FIND_PACKAGE
	FIND_PACKAGE(CURL)
	IF(CURL_FOUND)
		INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
		SET(requiredlibs ${requiredlibs} ${CURL_LIBRARIES} )
	ELSE(CURL_FOUND)
		MESSAGE(FATAL_ERROR "Could not find the CURL library and development files.")
	ENDIF(CURL_FOUND)
ENDIF(CURL_LIB AND CURL_INCLUDE_DIR)


IF(WIN32 AND NOT CYGWIN)
	IF (MSVC)
		MESSAGE(STATUS "In MSVC")
	ELSEIF(MINGW)
		MESSAGE(STATUS "In MINGW")
	ELSE (MSVC)
		MESSAGE(STATUS "In What?")
	ENDIF (MSVC)
ENDIF(WIN32 AND NOT CYGWIN)


FIND_LIBRARY(OPENSSL_LIB ssleay32)
FIND_PATH(OPENSSL_INCLUDE_DIR ssl.h PATH_SUFFIXES openssl)
IF(OPENSSL_LIB AND OPENSSL_INCLUDE_DIR)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
	#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${OPENSSL_LIB}")
	LINK_DIRECTORIES(${OPENSSL_LIB})
	SET(requiredlibs ${requiredlibs} "ssleay32" "libeay32")
	MESSAGE(STATUS "Found OpenSSL: ${OPENSSL_LIB} and ${OPENSSL_INCLUDE_DIR}")
ELSE(OPENSSL_LIB AND OPENSSL_INCLUDE_DIR)
	# Try again with FIND_PACKAGE
	FIND_PACKAGE(OpenSSL)
	IF(OPENSSL_FOUND)
		INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
		SET(requiredlibs ${requiredlibs} ${OPENSSL_LIBRARIES} ${LIBCRYPTO}) # bugfix, because libcrypto is not found automatically
	ELSE(OPENSSL_FOUND)
		MESSAGE(FATAL_ERROR "Could not find the OpenSSL library and development files.")
	ENDIF(OPENSSL_FOUND)
ENDIF(OPENSSL_LIB AND OPENSSL_INCLUDE_DIR)


FIND_LIBRARY(LIBXML2_LIB libxml2)
FIND_PATH(LIBXML2_INCLUDE_DIR xpath.h PATH_SUFFIXES libxml)
IF(LIBXML2_LIB AND LIBXML2_INCLUDE_DIR)
	INCLUDE_DIRECTORIES(${LIBXML2_INCLUDE_DIR})
	LINK_DIRECTORIES(${LIBXML2_LIB})
	SET(requiredlibs ${requiredlibs} "libxml2")
	MESSAGE(STATUS "Found LibXml2: ${LIBXML2_LIB} and ${LIBXML2_INCLUDE_DIR}")
ELSE(LIBXML2_LIB AND LIBXML2_INCLUDE_DIR)
	# Try again with FIND_PACKAGE
	FIND_PACKAGE(LibXml2)
	IF(LIBXML2_FOUND)
		INCLUDE_DIRECTORIES(${LIBXML2_INCLUDE_DIR})
		SET(requiredlibs ${requiredlibs} ${LIBXML2_LIBRARIES}) 
		SET(CXXFLAGS ${CXXFLAGS} ${LIBXML2_DEFINITONS}) # needed?
	ELSE(LIBXML2_FOUND)
		MESSAGE(FATAL_ERROR "Could not find the libxml2 library and development files.")
	ENDIF(LIBXML2_FOUND)
ENDIF(LIBXML2_LIB AND LIBXML2_INCLUDE_DIR)

INCLUDE (CheckIncludeFiles)

SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${PTHREADLIB})


FIND_LIBRARY(PTHREAD_LIB pthread)
FIND_PATH(PTHREAD_INCLUDE_DIR pthread.h)
IF(PTHREAD_LIB AND PTHREAD_INCLUDE_DIR)
	SET(LIBAWS_HAVE_PTHREAD_H 1)
	INCLUDE_DIRECTORIES(${PTHREAD_INCLUDE_DIR})
	LINK_DIRECTORIES(${PTHREAD_LIB})
	#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${PTHREAD_LIB}")
	SET(requiredlibs ${requiredlibs} "pthread")
ELSE(PTHREAD_LIB AND PTHREAD_INCLUDE_DIR)
	# Try again with CHECK_INCLUDE_FILES
	CHECK_INCLUDE_FILES(pthread.h LIBAWS_HAVE_PTHREAD_H)
	MESSAGE(STATUS "CMAKE REQUIRED INCLUDES:${CMAKE_REQUIRED_INCLUDES}")
	IF(NOT LIBAWS_HAVE_PTHREAD_H)
		MESSAGE(FATAL_ERROR "Could not find pthread development headers.")
	ELSE(NOT LIBAWS_HAVE_PTHREAD_H)
		SET(requiredlibs ${requiredlibs} ${LIBPTHREADS})
	ENDIF(NOT LIBAWS_HAVE_PTHREAD_H)
ENDIF(PTHREAD_LIB AND PTHREAD_INCLUDE_DIR)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)

INCLUDE(CMakeConfiguration.txt)
INCLUDE(CMakeCompiler.txt)

SET(LIBAWS_MAJOR_VERSION "0")
SET(LIBAWS_MINOR_VERSION "9")
SET(LIBAWS_PATCH_VERSION "3")

# set the include path to the API headers
# note that there also some generated headers files in the build directory
INCLUDE_DIRECTORIES(AFTER ${CMAKE_CURRENT_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(AFTER ${CMAKE_CURRENT_BINARY_DIR}/include)

FIND_PACKAGE(Fuse)
IF(FUSE_FOUND)
  MESSAGE(STATUS "Found Fuse. Building the s3fs module.")
ELSE(FUSE_FOUND)
  MESSAGE(STATUS "Fuse not found. Can't build the s3fs module.")
ENDIF(FUSE_FOUND)


# Tell CMake to process the sub-directories
ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tests)
ADD_SUBDIRECTORY(bin)
IF(FUSE_FOUND)
  ADD_SUBDIRECTORY(fuse)
ENDIF(FUSE_FOUND)
ADD_SUBDIRECTORY(doc)

# add the uninstall support
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeUninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeUninstall.cmake"
    @ONLY)
MESSAGE(STATUS "configured ${CMAKE_CURRENT_SOURCE_DIR}/CMakeUninstall.cmake.in --> ${CMAKE_CURRENT_BINARY_DIR}/CMakeUninstall.cmake")
ADD_CUSTOM_TARGET(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/CMakeUninstall.cmake")

# configure the cmake script to create a source package
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/CPackSourceConfig.cmake.in 
               ${CMAKE_BINARY_DIR}/CPackSourceConfig.cmake)
MESSAGE(STATUS "configured ${CMAKE_SOURCE_DIR}/CPackSourceConfig.cmake.in --> ${CMAKE_BINARY_DIR}/CPackSourceConfig.cmake")

# export stuff
INCLUDE(CMakeExportBuildSettings)
CMAKE_EXPORT_BUILD_SETTINGS(${CMAKE_BINARY_DIR}/LIBAWSBuildSettings.cmake)
EXPORT_LIBRARY_DEPENDENCIES(${CMAKE_BINARY_DIR}/LIBAWSDepends.cmake)

CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/LIBAWSConfig.cmake.in ${CMAKE_BINARY_DIR}/LIBAWSConfig.cmake)

INCLUDE(CMakeCPack.cmake)
