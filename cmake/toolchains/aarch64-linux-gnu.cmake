# toolchain file that can be used for cross-compiling AppImageKit using the respective AppImageBuild container

set(CMAKE_SYSTEM_NAME Linux CACHE STRING "" FORCE)
set(CMAKE_SYSTEM_PROCESSOR arm CACHE STRING "" FORCE)

set(triple aarch64-linux-gnu CACHE STRING "" FORCE)

set(CMAKE_C_COMPILER "${triple}-gcc" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER "${triple}-g++" CACHE STRING "" FORCE)

set(TOOLS_PREFIX "${triple}-" CACHE STRING "" FORCE)

set(DEPENDENCIES_CFLAGS "-I/deps/include" CACHE STRING "" FORCE)
set(DEPENDENCIES_CPPFLAGS "${DEPENDENCIES_CFLAGS}" CACHE STRING "" FORCE)
set(DEPENDENCIES_LDFLAGS "-L/deps/lib/" CACHE STRING "" FORCE)

# host = target system
# build = build system
# both must be specified
set(EXTRA_CONFIGURE_FLAGS "--host=${triple}" "--build=x86_64-pc-linux-gnu" "--target=${triple}" CACHE STRING "" FORCE)
