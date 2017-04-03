# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Windows)

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER amd64-mingw32msvc-gcc)
SET(CMAKE_CXX_COMPILER amd64-mingw32msvc-g++)
SET(CMAKE_RC_COMPILER amd64-mingw32msvc-windres)

# here is the target environment located
SET(CMAKE_FIND_ROOT_PATH  /usr/amd64-mingw32msvc /usr/bin/amd64-mingw32msvc )

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

