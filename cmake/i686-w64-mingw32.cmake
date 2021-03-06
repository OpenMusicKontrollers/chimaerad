# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR "i686")
set(TOOLCHAIN "i686-w64-mingw32")

# which compilers to use for C and C++
set(CMAKE_C_COMPILER "${TOOLCHAIN}-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN}-g++")
set(CMAKE_RC_COMPILER "${TOOLCHAIN}-windres")

# here is the target environment located
set(CMAKE_FIND_ROOT_PATH "/usr/${TOOLCHAIN}")

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(STATIC_Z "/opt/${TOOLCHAIN}/lib/libz.a")
set(STATIC_ZIP "/opt/${TOOLCHAIN}/lib/libzip.a")
set(STATIC_UV "/opt/${TOOLCHAIN}/lib/libuv.a")
set(LIBS ${LIBS} "-static-libgcc -lws2_32 -lpsapi -liphlpapi -Wl,-Bstatic -lwinpthread -luserenv")

set(WINE wine32)
